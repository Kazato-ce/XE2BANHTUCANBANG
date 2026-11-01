/**
 * @file uart_telemetry.h
 * @brief Truyền/nhận dữ liệu qua USART1 (PA9=TX, PA10=RX) sang ESP32-S3.
 *
 * Giao thức text, kết thúc bằng '\n', 115200 baud:
 *
 *  STM32 → ESP32 (telemetry, ~20 Hz):
 *    $T,<angle>,<rate>,<acc_angle>,<v_cmd>,<a_cmd>,<state>,<Kp>,<Ki>,<Kd>,<trim>,<fault>,<dt_us>\n
 *
 *  ESP32 → STM32 (điều chỉnh PID):
 *    $P,<Kp>,<Ki>,<Kd>,<trim>\n
 */
#ifndef UART_TELEMETRY_H
#define UART_TELEMETRY_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifndef HAL_UART_MODULE_ENABLED
/* Khai báo tương thích khi CubeMX chưa sinh HAL UART */
typedef struct {
    USART_TypeDef *Instance;
} UART_HandleTypeDef;
#endif

/**
 * @brief Khởi tạo module telemetry với handle USART1.
 * @param huart  Con trỏ USART1 handle (PA9 TX, PA10 RX)
 */
void uart_telemetry_init(UART_HandleTypeDef *huart);

/**
 * @brief Gửi một frame telemetry.
 *        Gọi từ app_loop() với tần suất ~20 Hz (mỗi 50 ms).
 *        Không blocking: dùng HAL_UART_Transmit với timeout ngắn.
 */
void uart_telemetry_send(void);

/**
 * @brief Xử lý byte nhận được (gọi trong HAL_UART_RxCpltCallback hoặc polling).
 *        Parse lệnh $P và cập nhật g_gains.
 * @param byte  Byte nhận được từ USART1
 */
void uart_telemetry_rx_byte(uint8_t byte);

/**
 * @brief Bật nhận interrupt 1 byte (gọi sau init và sau mỗi lần nhận xong).
 */
void uart_telemetry_start_rx(void);

/**
 * @brief Gọi từ HAL_UART_RxCpltCallback khi nhận xong 1 byte.
 */
void uart_telemetry_on_rx_complete(void);

#endif /* UART_TELEMETRY_H */

