/**
 * @file app_main.h
 * @brief Hook vào main.c: app_init(), app_loop(), và các HAL callback.
 */
#ifndef APP_MAIN_H
#define APP_MAIN_H

/**
 * @brief Gọi một lần trong USER CODE 2 (sau khi HAL init xong).
 */
void app_init(void);

/**
 * @brief Gọi liên tục trong while(1) — USER CODE 3.
 */
void app_loop(void);

#endif /* APP_MAIN_H */

