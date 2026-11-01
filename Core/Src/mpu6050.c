/**
 * @file mpu6050.c
 * @brief Driver MPU6050: init, đọc 14 byte, quy đổi đơn vị, hiệu chuẩn bias gyro.
 */
#include "mpu6050.h"
#include "app_config.h"
#include "main.h"
#include <math.h>
#include <string.h>

/* ──────── Thanh ghi MPU6050 ──────── */
#define REG_WHO_AM_I      0x75U
#define REG_PWR_MGMT_1    0x6BU
#define REG_CONFIG        0x1AU
#define REG_SMPLRT_DIV    0x19U
#define REG_GYRO_CONFIG   0x1BU
#define REG_ACCEL_CONFIG  0x1CU
#define REG_INT_PIN_CFG   0x37U
#define REG_INT_ENABLE    0x38U
#define REG_ACCEL_XOUT_H  0x3BU

/* Hệ số quy đổi */
#define ACCEL_LSB_PER_G   16384.0f   /* ±2g: 16384 LSB/g */
#define GYRO_LSB_PER_DPS  65.5f      /* ±500°/s: 65.5 LSB/(°/s) */

/* Timeout I2C ngắn (ms) */
#define I2C_TIMEOUT_MS    3U

/* ──────── Trạng thái nội bộ ──────── */
static I2C_HandleTypeDef *s_hi2c = NULL;

/* Bias gyro (LSB), trừ ra khi đọc */
static int16_t s_gyro_bias_x = 0;
static int16_t s_gyro_bias_y = 0;
static int16_t s_gyro_bias_z = 0;

/* Đếm lỗi I2C liên tiếp */
static uint32_t s_i2c_err_streak = 0;

/* ──────── Hàm phụ trợ nội bộ ──────── */

/** Ghi 1 byte vào thanh ghi */
static HAL_StatusTypeDef reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return HAL_I2C_Master_Transmit(s_hi2c, MPU6050_I2C_ADDR,
                                   buf, 2, I2C_TIMEOUT_MS);
}

/** Đọc n byte từ thanh ghi */
static HAL_StatusTypeDef reg_read(uint8_t reg, uint8_t *dst, uint16_t len)
{
    return HAL_I2C_Mem_Read(s_hi2c, MPU6050_I2C_ADDR,
                            reg, I2C_MEMADD_SIZE_8BIT,
                            dst, len, I2C_TIMEOUT_MS);
}

/** Nhấp SCL 9 lần để giải phóng bus I2C bị treo */
static void i2c_bus_recover_scl(void)
{
    /* Cấu hình tạm SCL (PB6) làm GPIO output để nhấp thủ công */
    GPIO_InitTypeDef g = {0};
    g.Pin  = GPIO_PIN_6;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &g);

    for (int i = 0; i < 9; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        HAL_Delay(1);
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_Delay(1);

    /* Trả lại chức năng I2C (HAL_I2C_MspInit sẽ cấu hình lại khi init lại) */
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);
}

/* ──────── API công khai ──────── */

MpuStatus_t mpu6050_init(I2C_HandleTypeDef *hi2c)
{
    s_hi2c = hi2c;
    uint8_t val;

    /* Kiểm tra WHO_AM_I */
    if (reg_read(REG_WHO_AM_I, &val, 1) != HAL_OK) return MPU_ERR_I2C;
    if (val != 0x68U) return MPU_ERR_WHOAMI;

    /* Reset toàn bộ MPU6050 */
    if (reg_write(REG_PWR_MGMT_1, 0x80U) != HAL_OK) return MPU_ERR_I2C;
    HAL_Delay(100);

    /* Chọn PLL theo gyro X làm clock */
    if (reg_write(REG_PWR_MGMT_1, 0x01U) != HAL_OK) return MPU_ERR_I2C;

    /* DLPF ~42 Hz (chế độ 3) — gyro nội bộ 1 kHz */
    if (reg_write(REG_CONFIG, 0x03U) != HAL_OK) return MPU_ERR_I2C;

    /* Tần số lấy mẫu = 1000 / (1 + 4) = 200 Hz */
    if (reg_write(REG_SMPLRT_DIV, 4U) != HAL_OK) return MPU_ERR_I2C;

    /* Gyro ±500°/s */
    if (reg_write(REG_GYRO_CONFIG, 0x08U) != HAL_OK) return MPU_ERR_I2C;

    /* Accel ±2g */
    if (reg_write(REG_ACCEL_CONFIG, 0x00U) != HAL_OK) return MPU_ERR_I2C;

    /* INT: tích cực cao, xung ngắn */
    if (reg_write(REG_INT_PIN_CFG, 0x00U) != HAL_OK) return MPU_ERR_I2C;

    /* Bật ngắt DATA_READY */
    if (reg_write(REG_INT_ENABLE, 0x01U) != HAL_OK) return MPU_ERR_I2C;

    s_i2c_err_streak = 0;
    return MPU_OK;
}

MpuStatus_t mpu6050_calibrate_gyro(void)
{
    int32_t sum_x = 0, sum_y = 0, sum_z = 0;
    int32_t min_x = 32767, max_x = -32768;
    int32_t min_y = 32767, max_y = -32768;
    int32_t min_z = 32767, max_z = -32768;

    uint8_t raw[14];

    for (uint32_t i = 0; i < GYRO_CALIB_SAMPLES; i++) {
        /* Chờ ~5 ms / mẫu (200 Hz) */
        HAL_Delay(5);

        if (reg_read(REG_ACCEL_XOUT_H, raw, 14) != HAL_OK) {
            return MPU_ERR_I2C;
        }

        /* Gyro nằm ở byte [8..13] */
        int16_t gx = (int16_t)((raw[8]  << 8) | raw[9]);
        int16_t gy = (int16_t)((raw[10] << 8) | raw[11]);
        int16_t gz = (int16_t)((raw[12] << 8) | raw[13]);

        sum_x += gx; sum_y += gy; sum_z += gz;

        if (gx < min_x) { min_x = gx; }
        if (gx > max_x) { max_x = gx; }
        if (gy < min_y) { min_y = gy; }
        if (gy > max_y) { max_y = gy; }
        if (gz < min_z) { min_z = gz; }
        if (gz > max_z) { max_z = gz; }
    }

    /* Kiểm tra dao động */
    if ((max_x - min_x) > GYRO_CALIB_NOISE_THR ||
        (max_y - min_y) > GYRO_CALIB_NOISE_THR ||
        (max_z - min_z) > GYRO_CALIB_NOISE_THR) {
        return MPU_ERR_CALIB;
    }

    s_gyro_bias_x = (int16_t)(sum_x / (int32_t)GYRO_CALIB_SAMPLES);
    s_gyro_bias_y = (int16_t)(sum_y / (int32_t)GYRO_CALIB_SAMPLES);
    s_gyro_bias_z = (int16_t)(sum_z / (int32_t)GYRO_CALIB_SAMPLES);

    return MPU_OK;
}

MpuStatus_t mpu6050_read(MpuData_t *data)
{
    uint8_t raw[14];

    if (reg_read(REG_ACCEL_XOUT_H, raw, 14) != HAL_OK) {
        s_i2c_err_streak++;
        return MPU_ERR_I2C;
    }
    s_i2c_err_streak = 0;

    /* Parse big-endian: ACCEL XYZ (byte 0-5), TEMP (6-7), GYRO XYZ (8-13) */
    int16_t ax_raw = (int16_t)((raw[0]  << 8) | raw[1]);
    int16_t ay_raw = (int16_t)((raw[2]  << 8) | raw[3]);
    int16_t az_raw = (int16_t)((raw[4]  << 8) | raw[5]);
    int16_t gx_raw = (int16_t)((raw[8]  << 8) | raw[9]);
    int16_t gy_raw = (int16_t)((raw[10] << 8) | raw[11]);
    int16_t gz_raw = (int16_t)((raw[12] << 8) | raw[13]);

    /* Trừ bias gyro */
    gx_raw -= s_gyro_bias_x;
    gy_raw -= s_gyro_bias_y;
    gz_raw -= s_gyro_bias_z;

    /* Quy đổi đơn vị */
    data->ax_g   = (float)ax_raw / ACCEL_LSB_PER_G;
    data->ay_g   = (float)ay_raw / ACCEL_LSB_PER_G;
    data->az_g   = (float)az_raw / ACCEL_LSB_PER_G;
    data->gx_dps = (float)gx_raw / GYRO_LSB_PER_DPS;
    data->gy_dps = (float)gy_raw / GYRO_LSB_PER_DPS;
    data->gz_dps = (float)gz_raw / GYRO_LSB_PER_DPS;

    return MPU_OK;
}

MpuStatus_t mpu6050_recover(void)
{
    /* Nhấp SCL 9 lần để giải phóng bus treo */
    i2c_bus_recover_scl();

    /* Khởi tạo lại ngoại vi I2C */
    if (HAL_I2C_DeInit(s_hi2c) != HAL_OK) return MPU_ERR_I2C;
    if (HAL_I2C_Init(s_hi2c)   != HAL_OK) return MPU_ERR_I2C;

    /* Init lại MPU6050 */
    return mpu6050_init(s_hi2c);
}

