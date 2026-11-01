/**
 * @file balance.c
 * @brief Máy trạng thái cân bằng:
 *        INIT → CALIBRATE → IDLE → BALANCE → FALLEN → IDLE
 *                                     ↓
 *                                   FAULT (lỗi IMU) → CALIBRATE (recover)
 */
#include "balance.h"
#include "app_config.h"
#include "mpu6050.h"
#include "attitude.h"
#include "pid.h"
#include "stepper.h"
#include "debug_vars.h"
#include "main.h"
#include <math.h>

/* ──────── Biến dùng chung với app_main.c ──────── */
extern app_debug_t g_dbg;
extern pid_gains_t g_gains;
/* Timestamp mẫu IMU cuối (cập nhật trong EXTI ISR, app_main.c) */
extern volatile uint32_t g_last_sample_ms;

/* ──────── Biến nội bộ ──────── */
static BalanceState_t s_state = STATE_INIT;
static PidContext_t   s_pid_ctx;
static float          s_v_cmd = 0.0f;   /* Tốc độ tích phân (steps/s) */

/* Đếm thời gian giữ thẳng trước khi bật motor */
static uint32_t s_idle_hold_start_ms = 0;
static bool     s_in_window_prev     = false;

/* Đếm thời gian không có mẫu IMU — extern để app_main.c cập nhật từ ISR */
volatile uint32_t g_last_sample_ms = 0;

/* Thử init lại định kỳ khi FAULT */
static uint32_t s_fault_retry_ms = 0;
#define FAULT_RETRY_INTERVAL_MS  500U

/* ──────── Kẹp float ──────── */
static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float fabsf_local(float x) { return x < 0.0f ? -x : x; }

/* ──────── Tắt motor khẩn cấp ──────── */
static void emergency_stop(void)
{
    stepper_enable(false);
    stepper_set_speed(MOTOR_1, 0.0f);
    stepper_set_speed(MOTOR_2, 0.0f);
    s_v_cmd = 0.0f;
}

/* ──────── API công khai ──────── */

void balance_init(void)
{
    pid_init(&s_pid_ctx, KI_INTEGRAL_MAX);
    s_state = STATE_INIT;
    s_v_cmd = 0.0f;
    s_in_window_prev = false;
    g_dbg.state = (int32_t)STATE_INIT;
    g_dbg.fault_flags = 0;
}

BalanceState_t balance_get_state(void)
{
    return s_state;
}

void balance_run(void)
{
    uint32_t now = HAL_GetTick();

    switch (s_state) {

    /* ── INIT: kiểm tra IMU ── */
    case STATE_INIT: {
        g_dbg.state = (int32_t)STATE_INIT;
        emergency_stop();

        extern I2C_HandleTypeDef hi2c1;
        MpuStatus_t st = mpu6050_init(&hi2c1);
        if (st == MPU_ERR_WHOAMI) {
            g_dbg.fault_flags |= FAULT_IMU_WHOAMI;
            s_state = STATE_FAULT;
        } else if (st != MPU_OK) {
            g_dbg.fault_flags |= FAULT_IMU_I2C;
            s_state = STATE_FAULT;
        } else {
            s_state = STATE_CALIBRATE;
        }
        break;
    }

    /* ── CALIBRATE: lấy bias gyro ── */
    case STATE_CALIBRATE: {
        g_dbg.state = (int32_t)STATE_CALIBRATE;
        emergency_stop();
        g_dbg.fault_flags &= ~(FAULT_IMU_WHOAMI | FAULT_IMU_I2C |
                                FAULT_CALIB_NOISE | FAULT_IMU_STALE);

        MpuStatus_t st = mpu6050_calibrate_gyro();
        if (st == MPU_ERR_CALIB) {
            g_dbg.fault_flags |= FAULT_CALIB_NOISE;
            /* Thử lại ngay */
        } else if (st != MPU_OK) {
            g_dbg.fault_flags |= FAULT_IMU_I2C;
            s_state = STATE_FAULT;
        } else {
            /* Lấy mẫu đầu tiên để khởi tạo filter */
            MpuData_t d;
            if (mpu6050_read(&d) == MPU_OK) {
                attitude_reset(&d);
                g_last_sample_ms = now;
            }
            s_state = STATE_IDLE;
        }
        break;
    }

    /* ── IDLE: chờ robot vào vùng cân bằng ── */
    case STATE_IDLE: {
        g_dbg.state = (int32_t)STATE_IDLE;

        /* Kiểm tra timeout mẫu IMU */
        if ((now - g_last_sample_ms) > IMU_STALE_MS) {
            g_dbg.fault_flags |= FAULT_IMU_STALE;
            emergency_stop();
            s_state = STATE_FAULT;
            break;
        }

        float angle = g_dbg.angle_deg;
        float setpoint = ANGLE_SETPOINT_DEG + g_gains.trim;
        bool in_window = fabsf_local(angle - setpoint) < START_WINDOW_DEG;

        if (!in_window) {
            s_in_window_prev = false;
            s_idle_hold_start_ms = now;
        } else {
            if (!s_in_window_prev) {
                /* Mới vào vùng: bắt đầu đếm thời gian */
                s_idle_hold_start_ms = now;
                s_in_window_prev = true;
            }
            /* Đã đủ thời gian giữ thẳng? */
            if ((now - s_idle_hold_start_ms) >= START_HOLD_MS) {
                /* Kiểm tra tick TIM3 trước khi bật motor */
                if (stepper_has_tick_error()) {
                    g_dbg.fault_flags |= FAULT_TICK_HZ;
                    s_state = STATE_FAULT;
                    break;
                }
                /* Bật motor, reset PID */
                pid_reset(&s_pid_ctx);
                s_v_cmd = 0.0f;
                stepper_set_speed(MOTOR_1, 0.0f);
                stepper_set_speed(MOTOR_2, 0.0f);
                stepper_enable(true);
                s_state = STATE_BALANCE;
                s_in_window_prev = false;
            }
        }
        break;
    }

    /* ── BALANCE: vòng điều khiển chính ── */
    case STATE_BALANCE: {
        g_dbg.state = (int32_t)STATE_BALANCE;

        /* Kiểm tra ngã */
        if (fabsf_local(g_dbg.angle_deg) > FALL_ANGLE_DEG) {
            emergency_stop();
            s_state = STATE_FALLEN;
            break;
        }

        /* Kiểm tra timeout mẫu */
        if ((now - g_last_sample_ms) > IMU_STALE_MS) {
            g_dbg.fault_flags |= FAULT_IMU_STALE;
            emergency_stop();
            s_state = STATE_FAULT;
            break;
        }

        /* Tính dt */
        float dt = attitude_measure_dt();

        /* Đọc Kp/Ki/Kd từ g_gains (áp dụng mỗi vòng để tune real-time) */
        float Kp = g_gains.Kp;
        float Ki = g_gains.Ki;
        float Kd = g_gains.Kd;
        float setpoint = ANGLE_SETPOINT_DEG + g_gains.trim;

        /* Tính a_cmd */
        float a_cmd = pid_compute(&s_pid_ctx,
                                  g_dbg.angle_deg,
                                  g_dbg.rate_dps,
                                  setpoint,
                                  Kp, Ki, Kd, dt);

        /* Tích phân v_cmd (gia tốc → tốc độ) */
        s_v_cmd += a_cmd * dt;
        s_v_cmd  = clampf(s_v_cmd, -V_MAX_SPS, V_MAX_SPS);

        /* Đặt tốc độ cho cả hai motor
         * Motor 1 (trái) và Motor 2 (phải) cùng chiều khi tiến */
        stepper_set_speed(MOTOR_1,  s_v_cmd);
        stepper_set_speed(MOTOR_2,  s_v_cmd);

        /* Ghi debug */
        g_dbg.a_cmd = a_cmd;
        g_dbg.v_cmd = s_v_cmd;
        break;
    }

    /* ── FALLEN: robot ngã, chờ dựng lại ── */
    case STATE_FALLEN: {
        g_dbg.state = (int32_t)STATE_FALLEN;

        /* Kiểm tra timeout mẫu */
        if ((now - g_last_sample_ms) > IMU_STALE_MS * 5U) {
            g_dbg.fault_flags |= FAULT_IMU_STALE;
            s_state = STATE_FAULT;
            break;
        }

        float angle = g_dbg.angle_deg;
        float setpoint = ANGLE_SETPOINT_DEG + g_gains.trim;
        if (fabsf_local(angle - setpoint) < START_WINDOW_DEG) {
            /* Về vùng cân bằng → về IDLE để kiểm tra lại */
            s_in_window_prev = false;
            s_idle_hold_start_ms = now;
            s_state = STATE_IDLE;
        }
        break;
    }

    /* ── FAULT: lỗi nghiêm trọng, thử recover định kỳ ── */
    case STATE_FAULT: {
        g_dbg.state = (int32_t)STATE_FAULT;
        emergency_stop();

        if ((now - s_fault_retry_ms) >= FAULT_RETRY_INTERVAL_MS) {
            s_fault_retry_ms = now;
            MpuStatus_t st = mpu6050_recover();
            if (st == MPU_OK) {
                g_dbg.fault_flags &= ~(FAULT_IMU_I2C | FAULT_IMU_STALE |
                                        FAULT_IMU_WHOAMI);
                s_state = STATE_CALIBRATE;
            }
        }
        break;
    }

    default:
        s_state = STATE_FAULT;
        break;
    }
}

/**
 * @brief Cập nhật dữ liệu IMU và góc — gọi từ app_loop() mỗi khi có mẫu mới.
 *        Tách riêng để balance_run() luôn dùng dữ liệu mới nhất.
 */
void balance_update_imu(const MpuData_t *data, float dt_s)
{
    extern app_debug_t g_dbg;
    float angle, rate;
    attitude_update(data, dt_s, &angle, &rate);

    /* Tính góc accel để debug */
    float acc_ang = atan2f((float)ANGLE_SIGN * data->ax_g,
                           (float)ANGLE_SIGN * data->az_g)
                    * (180.0f / 3.14159265f);

    g_dbg.angle_deg     = angle;
    g_dbg.rate_dps      = rate;
    g_dbg.acc_angle_deg = acc_ang;
}
