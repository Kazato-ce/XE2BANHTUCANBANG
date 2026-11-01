/**
 * @file pid.h
 * @brief Bộ điều khiển PD/PID với anti-windup cho robot tự cân bằng.
 */
#ifndef PID_H
#define PID_H

#include <stdint.h>

/* Context của bộ PID — một instance cho mỗi trục điều khiển */
typedef struct {
    float integral;     /* Tích phân tích lũy */
    float integral_max; /* Giới hạn tích phân (anti-windup) */
} PidContext_t;

/**
 * @brief Khởi tạo PID context, reset tích phân về 0.
 */
void pid_init(PidContext_t *ctx, float integral_max);

/**
 * @brief Reset tích phân về 0 (gọi khi chuyển sang BALANCE).
 */
void pid_reset(PidContext_t *ctx);

/**
 * @brief Tính đầu ra bộ điều khiển.
 *        a_cmd = Kp*e + Kd*rate + Ki*integral
 *        Đạo hàm lấy từ gyro (rate_dps), không vi phân số.
 *
 * @param ctx       Context PID
 * @param angle_deg Góc nghiêng hiện tại (degree)
 * @param rate_dps  Tốc độ góc gyro (°/s)
 * @param setpoint  Góc mục tiêu (degree)
 * @param Kp        Hệ số P
 * @param Ki        Hệ số I
 * @param Kd        Hệ số D
 * @param dt_s      Bước thời gian (giây)
 * @return          a_cmd (steps/s²)
 */
float pid_compute(PidContext_t *ctx,
                  float angle_deg, float rate_dps,
                  float setpoint,
                  float Kp, float Ki, float Kd,
                  float dt_s);

#endif /* PID_H */

