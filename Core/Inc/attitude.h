/**
 * @file attitude.h
 * @brief Complementary filter: tính góc nghiêng từ accel + gyro.
 */
#ifndef ATTITUDE_H
#define ATTITUDE_H

#include "mpu6050.h"

/**
 * @brief Khởi tạo filter, lấy góc ban đầu từ accelerometer.
 * @param data  Dữ liệu IMU mẫu đầu tiên
 */
void attitude_init(const MpuData_t *data);

/**
 * @brief Cập nhật complementary filter.
 * @param data      Dữ liệu IMU mới nhất
 * @param dt_s      Thời gian bước (giây), đo bằng DWT cycle counter
 * @param angle_out Góc nghiêng kết quả (degree), dương = nghiêng về trước
 * @param rate_out  Tốc độ góc từ gyro (°/s), dương = nghiêng về trước
 */
void attitude_update(const MpuData_t *data, float dt_s,
                     float *angle_out, float *rate_out);

/**
 * @brief Đo dt thực tế bằng DWT cycle counter.
 * @return dt tính bằng giây, kẹp trong [0.001, 0.020]
 */
float attitude_measure_dt(void);

/**
 * @brief Reset góc về giá trị tính từ accel hiện tại.
 */
void attitude_reset(const MpuData_t *data);

#endif /* ATTITUDE_H */

