/**
 * @file balance.h
 * @brief Máy trạng thái an toàn và vòng điều khiển cân bằng.
 */
#ifndef BALANCE_H
#define BALANCE_H

#include "debug_vars.h"

/**
 * @brief Khởi tạo máy trạng thái, bắt đầu từ STATE_INIT.
 */
void balance_init(void);

/**
 * @brief Chạy một bước của máy trạng thái + vòng điều khiển.
 *        Gọi từ app_loop() khi có mẫu IMU mới (cờ từ EXTI3).
 *        Tần suất ~200 Hz.
 */
void balance_run(void);

/**
 * @brief Lấy trạng thái hiện tại.
 */
BalanceState_t balance_get_state(void);

#endif /* BALANCE_H */

