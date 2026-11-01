/**
 * @file stepper.h
 * @brief Tạo xung STEP cho 2 motor TMC2208 qua DDS trong ngắt TIM3.
 */
#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

#ifndef HAL_TIM_MODULE_ENABLED
/* Khai báo tương thích khi CubeMX chưa bật HAL TIM */
typedef struct {
    TIM_TypeDef *Instance;
} TIM_HandleTypeDef;
#endif
#include "main.h"

/* Chỉ số motor */
typedef enum {
    MOTOR_1 = 0,  /* Bánh trái */
    MOTOR_2 = 1,  /* Bánh phải */
    MOTOR_COUNT = 2,
} MotorId_t;

/**
 * @brief Khởi tạo module stepper.
 *        Tính tick_hz từ clock thực tế của TIM3.
 *        Nếu lệch > 1% so với STEP_TICK_HZ_EXPECTED → đặt cờ lỗi.
 * @param htim  Con trỏ TIM3 handle
 */
void stepper_init(TIM_HandleTypeDef *htim);

/**
 * @brief Bật/tắt cả hai driver (EN = Low = bật, EN = High = tắt).
 * @param enable  true = bật motor
 */
void stepper_enable(bool enable);

/**
 * @brief Đặt tốc độ motor (steps/s), âm = đảo chiều.
 *        An toàn gọi từ main loop; ISR đọc atomically.
 * @param motor  MOTOR_1 hoặc MOTOR_2
 * @param sps    Tốc độ (steps/s), kẹp [-V_MAX_SPS, +V_MAX_SPS]
 */
void stepper_set_speed(MotorId_t motor, float sps);

/**
 * @brief Gọi trong HAL_TIM_PeriodElapsedCallback (TIM3, 20 kHz).
 *        Thực hiện DDS và tạo xung STEP.
 *        Dùng BSRR trực tiếp, không HAL, không delay.
 */
void stepper_tick_isr(void);

/**
 * @brief Trả về cờ lỗi tick_hz (true = lỗi).
 */
bool stepper_has_tick_error(void);

#endif /* STEPPER_H */
