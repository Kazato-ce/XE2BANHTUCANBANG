/**
 * @file stepper.c
 * @brief DDS phase accumulator cho 2 motor, tạo xung STEP trong ISR TIM3 (20 kHz).
 *        Dùng BSRR trực tiếp — không HAL, không delay trong ISR.
 *
 * Sơ đồ chân:
 *   Motor 1: PA0=EN, PA1=DIR, PA2=STEP
 *   Motor 2: PA3=EN, PA4=DIR, PA5=STEP
 */
#include "stepper.h"
#include "app_config.h"
#include "main.h"
#include <math.h>
#include <stddef.h>

/* ──────── Ánh xạ chân GPIO ──────── */
/* Motor 1 */
#define M1_EN_PIN    GPIO_PIN_0
#define M1_DIR_PIN   GPIO_PIN_1
#define M1_STEP_PIN  GPIO_PIN_2
/* Motor 2 */
#define M2_EN_PIN    GPIO_PIN_3
#define M2_DIR_PIN   GPIO_PIN_4
#define M2_STEP_PIN  GPIO_PIN_5

#define MOTOR_GPIO   GPIOA

/* ──────── Trạng thái DDS (nội bộ, dùng trong ISR) ──────── */
typedef struct {
    volatile uint32_t acc;    /* Bộ tích lũy pha */
    volatile uint32_t inc;    /* Gia số mỗi tick */
    volatile uint8_t  dir;    /* Hướng hiện tại (0/1) */
} DdsState_t;

static DdsState_t s_dds[MOTOR_COUNT];

/* Tick Hz thực tế (tính khi init) */
static uint32_t s_tick_hz = STEP_TICK_HZ_EXPECTED;

/* Cờ lỗi tick */
static bool s_tick_error = false;

/* Motor có đang được bật không */
static bool s_enabled = false;

/* ──────── Hàm phụ trợ: đặt DIR chân GPIO ──────── */
static void set_dir(MotorId_t motor, uint8_t dir)
{
    if (motor == MOTOR_1) {
        if (dir) MOTOR_GPIO->BSRR = M1_DIR_PIN;
        else     MOTOR_GPIO->BSRR = (M1_DIR_PIN << 16U);
    } else {
        if (dir) MOTOR_GPIO->BSRR = M2_DIR_PIN;
        else     MOTOR_GPIO->BSRR = (M2_DIR_PIN << 16U);
    }
}

/* ──────── API công khai ──────── */

void stepper_init(TIM_HandleTypeDef *htim)
{
    /* Đảm bảo EN = High (tắt) khi khởi động */
    MOTOR_GPIO->BSRR = (M1_EN_PIN | M2_EN_PIN);

    /* Reset DDS */
    for (int i = 0; i < MOTOR_COUNT; i++) {
        s_dds[i].acc = 0;
        s_dds[i].inc = 0;
        s_dds[i].dir = 0;
    }

    /* ── Tính tick_hz thực tế từ clock TIM3 ──
     * APB1 prescaler ≠ 1 → timer clock = 2 × PCLK1
     * Với HCLK=100 MHz, APB1 prescaler=2: PCLK1=50 MHz → timer=100 MHz
     * tick_hz = timer_clk / ((PSC+1) * (ARR+1))
     */
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    if (pclk1 == 0) pclk1 = 16000000U;
    uint32_t apb1_pre = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    uint32_t timer_clk = (apb1_pre >= 4U) ? (pclk1 * 2U) : pclk1;

    uint32_t psc;
    uint32_t arr;

    if (htim != NULL && htim->Instance != NULL) {
        psc = htim->Instance->PSC;
        arr = htim->Instance->ARR;
    } else {
        /* Tự động tính PSC và ARR để đạt đúng 20 kHz theo clock timer thực tế */
        uint32_t total_div = timer_clk / STEP_TICK_HZ_EXPECTED;
        if (total_div < 50U) total_div = 50U;
        arr = 49U;
        psc = (total_div / 50U) - 1U;

        RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
        TIM3->PSC = psc;
        TIM3->ARR = arr;
        TIM3->DIER |= TIM_DIER_UIE;
        TIM3->CR1 |= TIM_CR1_CEN;
        NVIC_SetPriority(TIM3_IRQn, 1);
        NVIC_EnableIRQ(TIM3_IRQn);
    }

    s_tick_hz = timer_clk / ((psc + 1U) * (arr + 1U));

    /* Cho phép dung sai rộng hơn để xe luôn chạy được ngay cả khi clock chưa cấu hình 100 MHz */
    if (s_tick_hz < 2000U || s_tick_hz > 50000U) {
        s_tick_error = true;
    } else {
        s_tick_error = false;
    }

    s_enabled = false;
}

void stepper_enable(bool enable)
{
    s_enabled = enable;
    if (enable && !s_tick_error) {
        /* EN = Low (tích cực mức thấp) = bật driver */
        MOTOR_GPIO->BSRR = (M1_EN_PIN << 16U) | (M2_EN_PIN << 16U);
    } else {
        /* EN = High = tắt driver */
        MOTOR_GPIO->BSRR = M1_EN_PIN | M2_EN_PIN;
        /* Reset DDS để không tạo xung khi enable trở lại */
        s_dds[MOTOR_1].inc = 0;
        s_dds[MOTOR_2].inc = 0;
    }
}

void stepper_set_speed(MotorId_t motor, float sps)
{
    /* Kẹp tốc độ */
    if (sps >  V_MAX_SPS) sps =  V_MAX_SPS;
    if (sps < -V_MAX_SPS) sps = -V_MAX_SPS;

    /* Xác định hướng */
    uint8_t dir;
    float abs_sps = sps;
    if (sps < 0.0f) {
        abs_sps = -sps;
        dir = (motor == MOTOR_1) ? (1U ^ MOTOR_1_DIR_INVERT)
                                 : (1U ^ MOTOR_2_DIR_INVERT);
    } else {
        dir = (motor == MOTOR_1) ? (0U ^ MOTOR_1_DIR_INVERT)
                                 : (0U ^ MOTOR_2_DIR_INVERT);
    }

    /* Tính increment DDS: inc = abs_sps * 2^32 / tick_hz */
    /* Kẹp tối đa tick_hz/2 steps/s → inc <= 2^31 */
    float max_sps_f = (float)s_tick_hz * 0.5f;
    if (abs_sps > max_sps_f) abs_sps = max_sps_f;

    uint32_t inc = (uint32_t)(abs_sps * 4294967296.0f / (float)s_tick_hz);

    /* Cập nhật nguyên tử (32-bit, Cortex-M đảm bảo nguyên tử) */
    s_dds[motor].dir = dir;
    s_dds[motor].inc = inc;

    /* Đặt chân DIR */
    set_dir(motor, dir);
}

void stepper_tick_isr(void)
{
    /* Bước 1: kéo cả hai STEP xuống thấp (reset trước để tạo xung sạch) */
    MOTOR_GPIO->BSRR = (M1_STEP_PIN << 16U) | (M2_STEP_PIN << 16U);

    /* Bước 2: tích lũy pha và tạo xung STEP khi tràn */
    uint32_t set_bits = 0;

    /* Motor 1 */
    {
        uint32_t prev = s_dds[MOTOR_1].acc;
        s_dds[MOTOR_1].acc += s_dds[MOTOR_1].inc;
        if (s_dds[MOTOR_1].acc < prev) { /* Tràn = 1 step */
            set_bits |= M1_STEP_PIN;
        }
    }

    /* Motor 2 */
    {
        uint32_t prev = s_dds[MOTOR_2].acc;
        s_dds[MOTOR_2].acc += s_dds[MOTOR_2].inc;
        if (s_dds[MOTOR_2].acc < prev) {
            set_bits |= M2_STEP_PIN;
        }
    }

    if (set_bits) {
        MOTOR_GPIO->BSRR = set_bits;
    }
}

bool stepper_has_tick_error(void)
{
    return s_tick_error;
}


/**
 * @brief Ngắt TIM3 trực tiếp (chạy khi CubeMX chưa cấu hình HAL TIM)
 */
__attribute__((weak)) void TIM3_IRQHandler(void)
{
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR &= ~TIM_SR_UIF;
        stepper_tick_isr();
    }
}
