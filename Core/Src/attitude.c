/**
 * @file attitude.c
 * @brief Complementary filter dùng DWT cycle counter để đo dt.
 *        Góc dương = robot nghiêng về phía trước.
 */
#include "attitude.h"
#include "app_config.h"
#include "stm32f4xx_hal.h"
#include <math.h>

/* ──────── Biến nội bộ ──────── */
static float   s_angle = 0.0f;   /* Góc hiện tại (degree) */
static uint32_t s_last_cyc = 0;  /* Chu kỳ DWT lần trước */

/* Giá trị kẹp dt (s) */
#define DT_MIN_S   0.001f
#define DT_MAX_S   0.020f

/* ──────── Tính góc từ accelerometer ──────── */
static float accel_angle(const MpuData_t *d)
{
    /* atan2(ax, az): góc nghiêng theo trục pitch
       ANGLE_SIGN đảo dấu nếu trục lắp ngược */
    float ang = atan2f((float)ANGLE_SIGN * d->ax_g,
                       (float)ANGLE_SIGN * d->az_g);
    return ang * (180.0f / 3.14159265f);
}

/* ──────── API công khai ──────── */

void attitude_init(const MpuData_t *data)
{
    /* Bật DWT cycle counter */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    s_angle   = accel_angle(data);
    s_last_cyc = DWT->CYCCNT;
}

float attitude_measure_dt(void)
{
    uint32_t now = DWT->CYCCNT;
    uint32_t delta = now - s_last_cyc;  /* Xử lý wrap-around 32-bit tự động */
    s_last_cyc = now;

    float dt = (float)delta / (float)SystemCoreClock;
    /* Kẹp dt trong khoảng hợp lý */
    if (dt < DT_MIN_S) dt = DT_MIN_S;
    if (dt > DT_MAX_S) dt = DT_MAX_S;
    return dt;
}

void attitude_update(const MpuData_t *data, float dt_s,
                     float *angle_out, float *rate_out)
{
    /* Tốc độ góc pitch (°/s) — ánh xạ trục và dấu qua ANGLE_SIGN */
    float rate = (float)ANGLE_SIGN * data->gy_dps;

    /* Góc từ accelerometer */
    float acc_ang = accel_angle(data);

    /* Hệ số filter */
    float tau = COMP_FILTER_TAU_S;
    float a   = tau / (tau + dt_s);

    /* Complementary filter */
    s_angle = a * (s_angle + rate * dt_s) + (1.0f - a) * acc_ang;

    *angle_out = s_angle;
    *rate_out  = rate;
}

void attitude_reset(const MpuData_t *data)
{
    s_angle = accel_angle(data);
}

