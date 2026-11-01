/**
 * @file app_config.h
 * @brief Tất cả tham số cấu hình phần cứng và điều khiển.
 *        Chỉnh sửa file này để tune robot — không rải số ma thuật trong code.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ──────────────────────────────────────────────
   Phần cứng động cơ
   ────────────────────────────────────────────── */
#define MOTOR_FULL_STEPS_PER_REV   200U       /* 1.8°/step */
#define MICROSTEP                  16U        /* Phải khớp với MS1/MS2 phần cứng TMC2208 */
#define STEPS_PER_REV              (MOTOR_FULL_STEPS_PER_REV * MICROSTEP)

/* Đảo hướng motor (0 = không đảo, 1 = đảo) */
#define MOTOR_1_DIR_INVERT         0
#define MOTOR_2_DIR_INVERT         1          /* Hai motor lắp đối xứng */

/* ──────────────────────────────────────────────
   Timer STEP (TIM3)
   ────────────────────────────────────────────── */
#define STEP_TICK_HZ_EXPECTED      20000U     /* 20 kHz tick TIM3 */
#define STEP_TICK_HZ_TOL_PCT       1U         /* Dung sai 1% */

/* Tốc độ tối đa (steps/s) — phải ≤ tick_hz/2 */
#define V_MAX_SPS                  8000.0f

/* ──────────────────────────────────────────────
   PID — giá trị khởi điểm, tune lại trên robot thật
   ────────────────────────────────────────────── */
#define KP_DEFAULT                 600.0f     /* steps/s² / degree */
#define KD_DEFAULT                 20.0f      /* steps/s² / (°/s) */
#define KI_DEFAULT                 0.0f       /* mặc định tắt */

/* Giới hạn tích phân (anti-windup) */
#define KI_INTEGRAL_MAX            2000.0f

/* Góc cân bằng cơ khí (degree). Chỉnh khi robot nghiêng sẵn. */
#define ANGLE_SETPOINT_DEG         0.0f

/* ──────────────────────────────────────────────
   Máy trạng thái an toàn
   ────────────────────────────────────────────── */
#define START_WINDOW_DEG           3.0f       /* Cửa sổ để bắt đầu cân bằng */
#define START_HOLD_MS              1000U      /* Thời gian giữ thẳng trước khi bật motor */
#define FALL_ANGLE_DEG             35.0f      /* Góc ngã: tắt motor ngay */
#define IMU_STALE_MS               20U        /* Quá thời gian không có mẫu → FAULT */

/* ──────────────────────────────────────────────
   Complementary filter
   ────────────────────────────────────────────── */
#define COMP_FILTER_TAU_S          0.5f       /* Hằng thời gian (giây) */

/* ──────────────────────────────────────────────
   Ánh xạ trục MPU6050
   Góc dương = robot nghiêng về phía trước
   ────────────────────────────────────────────── */
#define ANGLE_SIGN                 (+1)       /* Đảo dấu nếu cần: -1 */

/* ──────────────────────────────────────────────
   Địa chỉ I2C MPU6050
   ────────────────────────────────────────────── */
#define MPU6050_I2C_ADDR           (0x68U << 1U)

/* Số mẫu lấy trung bình khi hiệu chuẩn gyro */
#define GYRO_CALIB_SAMPLES         500U

/* Ngưỡng dao động cho phép khi hiệu chuẩn (LSB gyro) */
#define GYRO_CALIB_NOISE_THR       200

/* ──────────────────────────────────────────────
   Cờ kiểm tra: bật để test stepper riêng (P2)
   ────────────────────────────────────────────── */
/* #define APP_TEST_STEPPER */

#endif /* APP_CONFIG_H */

