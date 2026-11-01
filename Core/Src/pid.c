/**
 * @file pid.c
 * @brief Bộ điều khiển PD/PID, đạo hàm từ gyro, chống bão hòa tích phân.
 */
#include "pid.h"
#include <stddef.h>

/* Kẹp giá trị trong [lo, hi] */
static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void pid_init(PidContext_t *ctx, float integral_max)
{
    ctx->integral     = 0.0f;
    ctx->integral_max = integral_max;
}

void pid_reset(PidContext_t *ctx)
{
    ctx->integral = 0.0f;
}

float pid_compute(PidContext_t *ctx,
                  float angle_deg, float rate_dps,
                  float setpoint,
                  float Kp, float Ki, float Kd,
                  float dt_s)
{
    float e = angle_deg - setpoint;

    /* Tích phân với anti-windup */
    ctx->integral += e * dt_s;
    ctx->integral  = clampf(ctx->integral,
                            -ctx->integral_max,
                             ctx->integral_max);

    /* a_cmd = Kp*e + Kd*rate + Ki*integral */
    return Kp * e + Kd * rate_dps + Ki * ctx->integral;
}

