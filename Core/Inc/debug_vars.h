/**
 * @file debug_vars.h
 * @brief Biến debug dùng Live Expressions / Cortex-Debug.
 *        Chỉnh g_gains trực tiếp trong debugger để tune PID không cần flash lại.
 */
#ifndef DEBUG_VARS_H
#define DEBUG_VARS_H

#include <stdint.h>

/* Trạng thái máy trạng thái */
typedef enum {
    STATE_INIT      = 0,
    STATE_CALIBRATE = 1,
    STATE_IDLE      = 2,
    STATE_BALANCE   = 3,
    STATE_FALLEN    = 4,
    STATE_FAULT     = 5,
} BalanceState_t;

/* Cờ lỗi (bitmask) */
#define FAULT_IMU_WHOAMI    (1U << 0)  /* WHO_AM_I sai */
#define FAULT_IMU_I2C       (1U << 1)  /* Lỗi I2C liên tục */
#define FAULT_IMU_STALE     (1U << 2)  /* Không có mẫu quá lâu */
#define FAULT_CALIB_NOISE   (1U << 3)  /* Gyro dao động quá mức khi calib */
#define FAULT_TICK_HZ       (1U << 4)  /* Tick TIM3 lệch quá 1% */

/**
 * @brief Struct debug — xem trong Live Expressions.
 *        Tất cả trường volatile để debugger đọc đúng.
 */
typedef struct {
    volatile float  angle_deg;       /* Góc nghiêng hiện tại (degree) */
    volatile float  rate_dps;        /* Tốc độ góc từ gyro (°/s) */
    volatile float  acc_angle_deg;   /* Góc tính từ accelerometer (degree) */
    volatile float  a_cmd;           /* Gia tốc lệnh (steps/s²) */
    volatile float  v_cmd;           /* Tốc độ lệnh (steps/s) */
    volatile int32_t state;          /* Trạng thái máy trạng thái (BalanceState_t) */
    volatile uint32_t loop_dt_us;    /* Thời gian vòng điều khiển (µs) */
    volatile uint32_t i2c_err_count; /* Đếm lỗi I2C */
    volatile uint32_t fault_flags;   /* Bitmask lỗi FAULT_xxx */
} app_debug_t;

/**
 * @brief Gain PID — chỉnh trực tiếp trong debugger để tune.
 */
typedef struct {
    volatile float Kp;    /* Hệ số tỉ lệ */
    volatile float Ki;    /* Hệ số tích phân (thường = 0) */
    volatile float Kd;    /* Hệ số đạo hàm */
    volatile float trim;  /* Bù góc setpoint (degree) */
} pid_gains_t;

/* Khai báo extern — định nghĩa trong app_main.c */
extern app_debug_t g_dbg;
extern pid_gains_t g_gains;

#endif /* DEBUG_VARS_H */

