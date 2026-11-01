/**
 * @file app_main.c
 * @brief Điểm vào ứng dụng: app_init(), app_loop(), HAL callback.
 *
 * Hook tối thiểu trong main.c (USER CODE blocks):
 *   #include "app_main.h"   (Includes)
 *   app_init();                 (USER CODE 2)
 *   app_loop();                 (while(1), USER CODE 3)
 */
#include "app_main.h"
#include "app_config.h"
#include "debug_vars.h"
#include "mpu6050.h"
#include "attitude.h"
#include "stepper.h"
#include "balance.h"
#include "uart_telemetry.h"
#include "main.h"
#include <math.h>

/* ──────── Biến debug và gains (định nghĩa ở đây, extern ở debug_vars.h) ──────── */
app_debug_t g_dbg   = {0};
pid_gains_t g_gains = {
    .Kp   = KP_DEFAULT,
    .Ki   = KI_DEFAULT,
    .Kd   = KD_DEFAULT,
    .trim = 0.0f,
};

/* ──────── Handle ngoại vi (từ main.c) ──────── */
extern I2C_HandleTypeDef  hi2c1;
__attribute__((weak)) TIM_HandleTypeDef  htim3 = {0};
__attribute__((weak)) UART_HandleTypeDef huart1 = {0};   /* USART1: PA9=TX, PA10=RX → ESP32-S3 */

/* ──────── Cờ từ EXTI3 (MPU DATA_READY) ──────── */
volatile uint8_t g_imu_data_ready = 0;

/* ──────── Cờ từ TIM3 ──────── */
volatile uint8_t g_tim3_tick = 0;

/* ──────── Khai báo hàm nội bộ balance.c ──────── */
void balance_update_imu(const MpuData_t *data, float dt_s);

/* ──────── Biến đo thời gian vòng lặp ──────── */
static uint32_t s_loop_start_cyc = 0;

/* Timestamp gửi telemetry lần cuối */
static uint32_t s_telem_last_ms = 0;
#define TELEM_INTERVAL_MS   50U   /* 20 Hz */

/* ════════════════════════════════════════════════
   app_init — gọi một lần trong USER CODE 2
   ════════════════════════════════════════════════ */
void app_init(void)
{
    /* Bật DWT CYCCNT (cũng làm trong attitude_init, nhưng bật sớm là an toàn) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    /* Khởi tạo stepper (tính tick_hz, giữ EN=High) */
    stepper_init(&htim3);

    /* Khởi tạo máy trạng thái cân bằng */
    balance_init();

#ifdef HAL_TIM_MODULE_ENABLED
    /* Bật TIM3 nếu CubeMX đã sinh HAL TIM */
    if (htim3.Instance != NULL) {
        HAL_TIM_Base_Start_IT(&htim3);
    }
#endif

    /* Bật NVIC cho EXTI3 (MPU INT) — CubeMX đã cấu hình nhưng chưa bật IT */
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);

    /* Khởi tạo UART telemetry (USART1: PA9 TX, PA10 RX) */
    uart_telemetry_init(&huart1);
    uart_telemetry_start_rx();

#ifdef APP_TEST_STEPPER
    /* Chế độ test stepper P2: bật ngay không cần cân bằng */
    stepper_enable(true);
#endif
}

/* ════════════════════════════════════════════════
   app_loop — gọi liên tục trong while(1)
   ════════════════════════════════════════════════ */
void app_loop(void)
{
    s_loop_start_cyc = DWT->CYCCNT;

#ifdef APP_TEST_STEPPER
    /* ── P2: Test stepper ── */
    /* Motor 1 và 2 lần lượt: +2000 sps (2s), -2000 sps (2s), dừng */
    static uint32_t test_t = 0;
    uint32_t now = HAL_GetTick();
    if (now - test_t >= 2000U) {
        test_t = now;
        static int phase = 0;
        switch (phase % 3) {
        case 0:
            stepper_set_speed(MOTOR_1,  2000.0f);
            stepper_set_speed(MOTOR_2,  2000.0f);
            break;
        case 1:
            stepper_set_speed(MOTOR_1, -2000.0f);
            stepper_set_speed(MOTOR_2, -2000.0f);
            break;
        case 2:
            stepper_set_speed(MOTOR_1, 0.0f);
            stepper_set_speed(MOTOR_2, 0.0f);
            break;
        }
        phase++;
    }

#else
    /* ── Chế độ cân bằng bình thường ── */
    if (g_imu_data_ready) {
        g_imu_data_ready = 0;

        MpuData_t d;
        uint32_t t0 = DWT->CYCCNT;

        if (mpu6050_read(&d) == MPU_OK) {
            float dt_s = attitude_measure_dt();
            balance_update_imu(&d, dt_s);
            g_dbg.i2c_err_count = 0;
        } else {
            g_dbg.i2c_err_count++;
        }

        balance_run();

        /* Đo thời gian vòng lặp */
        uint32_t dt_cyc = DWT->CYCCNT - t0;
        g_dbg.loop_dt_us = dt_cyc / (SystemCoreClock / 1000000U);
    }

    /* Gửi telemetry 20 Hz (không phụ thuộc vào IMU) */
    {
        uint32_t now_ms = HAL_GetTick();
        if ((now_ms - s_telem_last_ms) >= TELEM_INTERVAL_MS) {
            s_telem_last_ms = now_ms;
            uart_telemetry_send();
        }
    }
#endif
}

/* ════════════════════════════════════════════════
   HAL Callbacks — gộp vào USER CODE 4 trong main.c
   hoặc giữ ở đây nếu chưa có trong main.c
   ════════════════════════════════════════════════ */

/**
 * @brief EXTI3 callback: MPU6050 DATA_READY (200 Hz).
 *        Chỉ đặt cờ, không I2C trong ISR.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_3) {
        g_imu_data_ready = 1;
        /* Cập nhật timestamp mẫu (32-bit HAL tick, nguyên tử) */
        extern volatile uint32_t g_last_sample_ms;
        g_last_sample_ms = HAL_GetTick();
    }
}

#ifdef HAL_TIM_MODULE_ENABLED
/**
 * @brief TIM3 overflow callback (20 kHz): tạo xung STEP.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3) {
        stepper_tick_isr();
    }
}
#endif

#ifdef HAL_UART_MODULE_ENABLED
/**
 * @brief USART1 RX interrupt callback: nhận 1 byte từ ESP32.
 *        Parse lệnh PID và bật lại interrupt để nhận byte tiếp theo.
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        uart_telemetry_on_rx_complete();
    }
}
#endif
