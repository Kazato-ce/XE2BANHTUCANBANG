/**
 * @file config.h
 * @brief Cấu hình WiFi và chân kết nối UART cho ESP32-S3 Super Mini
 */
#ifndef CONFIG_H
#define CONFIG_H

// ──────────────────────────────────────────────
// Cấu hình UART kết nối STM32F412
// ──────────────────────────────────────────────
// Sơ đồ nối dây:
//   STM32 PA9  (TX)  ----->  ESP32-S3 RXD (mặc định GPIO 44 hoặc GPIO 18)
//   STM32 PA10 (RX)  <-----  ESP32-S3 TXD (mặc định GPIO 43 hoặc GPIO 17)
//   STM32 GND        <=====> ESP32-S3 GND (Bắt buộc nối chung Mass GND!)
#define STM32_RX_PIN       44    // Chân RX của ESP32-S3 nhận từ PA9 của STM32
#define STM32_TX_PIN       43    // Chân TX của ESP32-S3 gửi sang PA10 của STM32
#define STM32_BAUDRATE     115200

// ──────────────────────────────────────────────
// Cấu hình Mạng Local (WiFi STA mode)
// Đổi tên WiFi và mật khẩu mạng nhà/phòng trọ của bạn:
// ──────────────────────────────────────────────
#define WIFI_STA_SSID      "YOUR_WIFI_SSID"      // Nhập tên WiFi nhà bạn
#define WIFI_STA_PASS      "YOUR_WIFI_PASSWORD"  // Nhập mật khẩu WiFi

// ──────────────────────────────────────────────
// Cấu hình Điểm phát sóng phụ (WiFi Access Point)
// Khi không có mạng nhà, ESP32 tự phát WiFi riêng này:
// IP mặc định khi kết nối AP: http://192.168.4.1
// ──────────────────────────────────────────────
#define WIFI_AP_SSID       "Robot_CanBang_AP"
#define WIFI_AP_PASS       "12345678"

// Tên miền cục bộ mDNS (truy cập: http://robot.local)
#define MDNS_NAME          "robot"

#endif // CONFIG_H

