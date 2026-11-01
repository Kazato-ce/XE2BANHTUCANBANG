/**
 * @file mpu6050.h
 * @brief Driver MPU6050: init, đọc raw, quy đổi đơn vị, hiệu chuẩn bias gyro.
 */
#ifndef MPU6050_H
#define MPU6050_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ──────── Kết quả trả về ──────── */
typedef enum {
    MPU_OK         = 0,
    MPU_ERR_WHOAMI = 1,  /* WHO_AM_I không khớp */
    MPU_ERR_I2C    = 2,  /* Lỗi giao tiếp I2C */
    MPU_ERR_CALIB  = 3,  /* Dao động quá mức khi hiệu chuẩn */
} MpuStatus_t;

/* ──────── Dữ liệu IMU đã quy đổi ──────── */
typedef struct {
    float ax_g;   /* Gia tốc trục X (g) */
    float ay_g;   /* Gia tốc trục Y (g) */
    float az_g;   /* Gia tốc trục Z (g) */
    float gx_dps; /* Tốc độ góc trục X (°/s) */
    float gy_dps; /* Tốc độ góc trục Y (°/s) */
    float gz_dps; /* Tốc độ góc trục Z (°/s) */
} MpuData_t;

/**
 * @brief Khởi tạo MPU6050: reset, cấu hình DLPF/SMPLRT/range, bật ngắt DATA_READY.
 * @param hi2c  Con trỏ đến handle I2C1
 * @return MPU_OK nếu thành công
 */
MpuStatus_t mpu6050_init(I2C_HandleTypeDef *hi2c);

/**
 * @brief Hiệu chuẩn bias gyro khi robot đứng yên.
 *        Hàm blocking (~2.5 s). Gọi sau mpu6050_init().
 * @return MPU_OK nếu thành công, MPU_ERR_CALIB nếu dao động quá lớn
 */
MpuStatus_t mpu6050_calibrate_gyro(void);

/**
 * @brief Đọc 14 byte raw (blocking) và quy đổi sang đơn vị vật lý.
 *        Đã trừ bias gyro. Chỉ gọi từ main loop, không gọi trong ISR.
 * @param data  Kết quả trả về
 * @return MPU_OK nếu thành công
 */
MpuStatus_t mpu6050_read(MpuData_t *data);

/**
 * @brief Thử khởi tạo lại I2C + MPU6050 sau khi lỗi.
 *        Nhấp SCL 9 lần để giải phóng bus trước khi init lại.
 * @return MPU_OK nếu thành công
 */
MpuStatus_t mpu6050_recover(void);

#endif /* MPU6050_H */

