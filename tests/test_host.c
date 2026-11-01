/**
 * @file test_host.c
 * @brief Unit test chạy trên host (gcc) cho:
 *        - Bộ tích lũy pha DDS (stepper)
 *        - PID compute (pid)
 *        - Complementary filter (attitude — phần toán học thuần)
 *
 * Biên dịch:  gcc -Wall -Wextra -lm -o test_host tests/test_host.c
 * Chạy:       ./test_host
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

/* ──────────────────────────────────────────────
   Stub / shim để test logic thuần không cần HAL
   ────────────────────────────────────────────── */

/* Định nghĩa lại các macro config cần dùng */
#define V_MAX_SPS              8000.0f
#define MOTOR_1_DIR_INVERT     0
#define MOTOR_2_DIR_INVERT     1
#define COMP_FILTER_TAU_S      0.5f
#define ANGLE_SIGN             (+1)
#define KI_INTEGRAL_MAX        2000.0f

/* ══════════════════════════════════════════════
   1. DDS phase accumulator (stepper logic)
   ══════════════════════════════════════════════ */

/**
 * Tính số bước thực tế tạo ra bởi DDS sau N tick.
 * Kết quả phải xấp xỉ sps * (N / tick_hz) ± 1 bước.
 */
static int dds_count_steps(float sps, uint32_t tick_hz, uint32_t n_ticks)
{
    if (sps > V_MAX_SPS) sps = V_MAX_SPS;
    if (sps < 0.0f) sps = -sps;

    float max_sps_f = (float)tick_hz * 0.5f;
    if (sps > max_sps_f) sps = max_sps_f;

    uint32_t inc = (uint32_t)(sps * 4294967296.0 / (double)tick_hz);
    uint32_t acc = 0;
    int steps = 0;

    for (uint32_t i = 0; i < n_ticks; i++) {
        uint32_t prev = acc;
        acc += inc;
        if (acc < prev) steps++;
    }
    return steps;
}

static bool test_dds(void)
{
    bool ok = true;
    uint32_t tick_hz = 20000;

    /* 2000 sps trong 2 s = 4000 bước (cho phép ±2) */
    {
        int steps = dds_count_steps(2000.0f, tick_hz, tick_hz * 2);
        int expected = 4000;
        if (abs(steps - expected) > 2) {
            printf("FAIL dds 2000sps: got %d expected %d\n", steps, expected);
            ok = false;
        } else {
            printf("PASS dds 2000sps: %d steps in 2s (expected %d)\n", steps, expected);
        }
    }

    /* 8000 sps trong 1 s = 8000 bước */
    {
        int steps = dds_count_steps(8000.0f, tick_hz, tick_hz);
        int expected = 8000;
        if (abs(steps - expected) > 2) {
            printf("FAIL dds 8000sps: got %d expected %d\n", steps, expected);
            ok = false;
        } else {
            printf("PASS dds 8000sps: %d steps in 1s (expected %d)\n", steps, expected);
        }
    }

    /* Kẹp: 20000 sps phải bị kẹp xuống 10000 (tick/2) */
    {
        int steps = dds_count_steps(20000.0f, tick_hz, tick_hz);
        int expected = 10000;
        if (abs(steps - expected) > 2) {
            printf("FAIL dds clamp: got %d expected %d\n", steps, expected);
            ok = false;
        } else {
            printf("PASS dds clamp: %d steps (capped at tick/2)\n", steps);
        }
    }

    return ok;
}

/* ══════════════════════════════════════════════
   2. PID compute
   ══════════════════════════════════════════════ */

typedef struct {
    float integral;
    float integral_max;
} PidCtx;

static void pid_init_host(PidCtx *c, float imax)
{
    c->integral = 0.0f;
    c->integral_max = imax;
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float pid_compute_host(PidCtx *c,
                               float angle, float rate,
                               float setpoint,
                               float Kp, float Ki, float Kd,
                               float dt)
{
    float e = angle - setpoint;
    c->integral += e * dt;
    c->integral  = clampf(c->integral, -c->integral_max, c->integral_max);
    return Kp * e + Kd * rate + Ki * c->integral;
}

static bool test_pid(void)
{
    bool ok = true;
    PidCtx ctx;
    float Kp = 600.0f, Ki = 0.0f, Kd = 20.0f;
    float dt = 0.005f; /* 200 Hz */

    /* Robot nghiêng 5° về phía trước, gyro = 0 → a_cmd = Kp * 5 = 3000 */
    {
        pid_init_host(&ctx, KI_INTEGRAL_MAX);
        float a = pid_compute_host(&ctx, 5.0f, 0.0f, 0.0f, Kp, Ki, Kd, dt);
        float expected = 3000.0f;
        if (fabsf(a - expected) > 1.0f) {
            printf("FAIL pid pure P: got %.2f expected %.2f\n", a, expected);
            ok = false;
        } else {
            printf("PASS pid pure P: a_cmd=%.2f (expected %.2f)\n", a, expected);
        }
    }

    /* D term: angle=0, gyro=10 dps → a_cmd = Kd*10 = 200 */
    {
        pid_init_host(&ctx, KI_INTEGRAL_MAX);
        float a = pid_compute_host(&ctx, 0.0f, 10.0f, 0.0f, Kp, Ki, Kd, dt);
        float expected = 200.0f;
        if (fabsf(a - expected) > 1.0f) {
            printf("FAIL pid pure D: got %.2f expected %.2f\n", a, expected);
            ok = false;
        } else {
            printf("PASS pid pure D: a_cmd=%.2f (expected %.2f)\n", a, expected);
        }
    }

    /* Anti-windup: Ki=10, integral phải bị kẹp tại KI_INTEGRAL_MAX */
    {
        pid_init_host(&ctx, KI_INTEGRAL_MAX);
        float Ki2 = 10.0f;
        /* Tích phân lớn (1000 bước) */
        for (int i = 0; i < 1000; i++) {
            pid_compute_host(&ctx, 1.0f, 0.0f, 0.0f, Kp, Ki2, 0.0f, dt);
        }
        if (ctx.integral > KI_INTEGRAL_MAX + 0.01f ||
            ctx.integral < -KI_INTEGRAL_MAX - 0.01f) {
            printf("FAIL pid anti-windup: integral=%.2f limit=%.2f\n",
                   ctx.integral, KI_INTEGRAL_MAX);
            ok = false;
        } else {
            printf("PASS pid anti-windup: integral=%.2f (limit %.2f)\n",
                   ctx.integral, KI_INTEGRAL_MAX);
        }
    }

    return ok;
}

/* ══════════════════════════════════════════════
   3. Complementary filter (toán học thuần)
   ══════════════════════════════════════════════ */

static float comp_filter(float prev_angle, float rate_dps,
                          float acc_angle, float dt)
{
    float tau = COMP_FILTER_TAU_S;
    float a   = tau / (tau + dt);
    return a * (prev_angle + rate_dps * dt) + (1.0f - a) * acc_angle;
}

static bool test_comp_filter(void)
{
    bool ok = true;

    /* Trạng thái ổn định: angle=10°, rate=0, acc_angle=10° → kết quả = 10° */
    {
        float dt   = 0.005f;
        float angle = 10.0f;
        for (int i = 0; i < 1000; i++) {
            angle = comp_filter(angle, 0.0f, 10.0f, dt);
        }
        if (fabsf(angle - 10.0f) > 0.01f) {
            printf("FAIL comp_filter steady: angle=%.4f expected 10.0\n", angle);
            ok = false;
        } else {
            printf("PASS comp_filter steady: angle=%.4f\n", angle);
        }
    }

    /* Accel bị nhiễu, gyro chính xác: góc phải gần với gyro */
    {
        float dt     = 0.005f;
        float angle  = 0.0f;
        float rate   = 10.0f;    /* 10°/s */
        float t_total = 1.0f;    /* 1 giây */
        int n = (int)(t_total / dt);
        for (int i = 0; i < n; i++) {
            /* Accel luôn báo 0° (nhiễu/sai) */
            angle = comp_filter(angle, rate, 0.0f, dt);
        }
        /* Kết quả phải gần gyro: 10°/s * 1s = 10° nhưng accel kéo về 0 */
        /* Với tau=0.5, sau 1s filter vẫn gần 10*(1-e^(-1/0.5))~8.6° */
        float gyro_pred = 10.0f * (1.0f - expf(-t_total / COMP_FILTER_TAU_S));
        if (fabsf(angle - gyro_pred) > 0.5f) {
            printf("FAIL comp_filter gyro_dom: angle=%.4f expected ~%.4f\n",
                   angle, gyro_pred);
            ok = false;
        } else {
            printf("PASS comp_filter gyro_dom: angle=%.4f (expected ~%.4f)\n",
                   angle, gyro_pred);
        }
    }

    return ok;
}

/* ══════════════════════════════════════════════
   4. UART telemetry frame parser ($P,Kp,Ki,Kd,trim\n)
   ══════════════════════════════════════════════ */
static bool test_telemetry_parser(void)
{
    bool ok = true;
    const char *frame = "$P,750.50,1.20,25.00,0.50\n";

    const char *p = frame + 3;
    char *end;
    float kp = strtof(p, &end); p = end + 1;
    float ki = strtof(p, &end); p = end + 1;
    float kd = strtof(p, &end); p = end + 1;
    float tr = strtof(p, &end);

    if (fabsf(kp - 750.50f) < 0.01f &&
        fabsf(ki - 1.20f) < 0.01f &&
        fabsf(kd - 25.00f) < 0.01f &&
        fabsf(tr - 0.50f) < 0.01f) {
        printf("PASS telemetry parser: Kp=%.2f Ki=%.2f Kd=%.2f trim=%.2f\n", kp, ki, kd, tr);
    } else {
        printf("FAIL telemetry parser: got Kp=%.2f Ki=%.2f Kd=%.2f trim=%.2f\n", kp, ki, kd, tr);
        ok = false;
    }
    return ok;
}

/* ══════════════════════════════════════════════
   main
   ══════════════════════════════════════════════ */
int main(void)
{
    printf("=== Unit Tests: robot tu can bang ===\n\n");

    bool all_ok = true;

    printf("-- DDS phase accumulator --\n");
    all_ok &= test_dds();

    printf("\n-- PID controller --\n");
    all_ok &= test_pid();

    printf("\n-- Complementary filter --\n");
    all_ok &= test_comp_filter();

    printf("\n-- Telemetry Parser --\n");
    all_ok &= test_telemetry_parser();

    printf("\n%s\n", all_ok ? "=== ALL TESTS PASSED ===" : "=== SOME TESTS FAILED ===");
    return all_ok ? 0 : 1;
}

