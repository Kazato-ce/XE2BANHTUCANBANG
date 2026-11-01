# Hướng Dẫn ESP32-S3 Super Mini — Telemetry & Điều Chỉnh PID Web Dashboard

Folder này chứa toàn bộ mã nguồn firmware cho **ESP32-S3 Super Mini**, dùng để kết nối với **STM32F412** qua UART, phát Web Server nội bộ (mạng Local hoặc Access Point) để theo dõi góc nghiêng MPU6050 thời gian thực và chỉnh các hệ số PID (Kp, Ki, Kd, Trim).

---

## 1. Sơ Đồ Đấu Dây (Wiring)

| STM32F412 Pin | Chức năng STM32 | Nối sang ESP32-S3 Super Mini | Ghi chú |
|---|---|---|---|
| **PA9** | USART1_TX | **RXD** (Mặc định GPIO 44 hoặc GPIO 18) | STM32 gửi dữ liệu MPU6050 sang ESP32 |
| **PA10** | USART1_RX | **TXD** (Mặc định GPIO 43 hoặc GPIO 17) | ESP32 gửi lệnh chỉnh PID về STM32 |
| **GND** | Mass | **GND** | **BẮT BUỘC NỐI CHUNG GND ĐỂ KHÔNG NHIỄU TÍN HIỆU** |
| 5V / 3V3 | Nguồn | 5V / 3V3 | Cấp nguồn cho ESP32 |

> [!IMPORTANT]
> - Nếu mạch ESP32-S3 Super Mini của bạn in nhãn chân khác, bạn có thể đổi chân trong file [config.h](file:///c:/Users/Thanh/Documents/Thực%20Hành%20Thiết%20Kế%20Hệ%20Thông%20Nhúng/ĐỒ%20ÁN/XE2BANHTUCANBANG/ESP32_SuperMini/config.h) (`STM32_RX_PIN` và `STM32_TX_PIN`).

---

## 2. Cách Bật USART1 Trong STM32CubeMX (Trên STM32F412)

Để STM32 kích hoạt chân PA9 và PA10 kết nối với ESP32:
1. Mở file `XE2BANHTUCANBANG.ioc` bằng **STM32CubeMX**.
2. Chọn **Connectivity** &rarr; **USART1**:
   - Mode: **Asynchronous**
   - Baud Rate: **115200** Bits/s
   - Word Length: **8 Bits**
   - Parity: **None**
   - Stop Bits: **1**
3. Trong tab **GPIO Settings** của USART1:
   - PA9: `USART1_TX`
   - PA10: `USART1_RX`
4. Trong tab **NVIC Settings**:
   - Tích chọn **Enabled** cho `USART1 global interrupt`.
5. Bấm **GENERATE CODE**.

---

## 3. Cách Nạp Code Cho ESP32-S3 Super Mini

### Cách A: Dùng Arduino IDE (Dễ nhất)
1. Cài đặt board ESP32 trong Arduino IDE:
   - Vào `File` &rarr; `Preferences` &rarr; dán URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` vào mục *Additional Boards Manager URLs*.
   - Vào `Tools` &rarr; `Board` &rarr; `Boards Manager` &rarr; tìm `esp32` và cài đặt.
2. Mở file [ESP32_SuperMini.ino](file:///c:/Users/Thanh/Documents/Thực%20Hành%20Thiết%20Kế%20Hệ%20Thông%20Nhúng/ĐỒ%20ÁN/XE2BANHTUCANBANG/ESP32_SuperMini/ESP32_SuperMini.ino).
3. Đổi thông tin WiFi nhà bạn trong file [config.h](file:///c:/Users/Thanh/Documents/Thực%20Hành%20Thiết%20Kế%20Hệ%20Thông%20Nhúng/ĐỒ%20ÁN/XE2BANHTUCANBANG/ESP32_SuperMini/config.h) (`WIFI_STA_SSID` và `WIFI_STA_PASS`).
4. Cài đặt thông số trong menu `Tools`:
   - **Board**: `"ESP32S3 Dev Module"`
   - **USB CDC On Boot**: `"Enabled"` (để in Serial ra cổng Type-C)
   - **Flash Size**: `"4MB"`
   - Chọn đúng cổng **Port** COM của ESP32.
5. Nhấn **Upload**.

### Cách B: Dùng PlatformIO (VS Code)
1. Mở thư mục `ESP32_SuperMini` bằng VS Code đã cài extension PlatformIO.
2. Bấm nút check `Build` hoặc mũi tên `Upload` ở thanh công cụ phía dưới.

---

## 4. Cách Truy Cập Web Dashboard & Điều Chỉnh Xe

Có 2 cách kết nối vào mạng điều khiển:

### Cách 1: Qua Mạng WiFi Local (Cùng mạng WiFi nhà / phòng Lab)
- Khi ESP32 kết nối vào WiFi nhà, mở Serial Monitor 115200 để xem IP (ví dụ: `192.168.1.45`).
- Mở trình duyệt trên điện thoại hoặc máy tính, gõ:
  ```
  http://192.168.1.xxx
  ```
  hoặc gõ:
  ```
  http://robot.local
  ```

### Cách 2: Qua Điểm Phát WiFi Riêng Của Robot (Đi ra ngoài sân/hành lang)
- Dùng điện thoại tìm mạng WiFi: **`Robot_CanBang_AP`**
- Mật khẩu: **`12345678`**
- Mở trình duyệt web gõ địa chỉ:
  ```
  http://192.168.4.1
  ```

---

## 5. Các Tính Năng Trên Giao Diện Web

1. **Giám sát MPU6050 & Chuyển động Real-time**:
   - Góc nghiêng Pitch (&theta;), Tốc độ góc Gyro (&deg;/s), Góc đo từ Accelerometer.
   - Tốc độ động cơ hiện tại (`v_cmd` steps/s), gia tốc lệnh (`a_cmd`).
   - Thời gian vòng quét điều khiển (`dt_us`).
   - Huy hiệu trạng thái robot: `INIT`, `CALIBRATE`, `IDLE`, `BALANCE` (xanh), `FALLEN` (vàng ngã xe), `FAULT` (đỏ lỗi cảm biến).
2. **Đồ thị thời gian thực (Real-time Chart)**:
   - Vẽ đường góc nghiêng Pitch (xanh) so với điểm cân bằng Setpoint (trắng gạch đứt).
3. **Thanh trượt & Ô nhập PID thời gian thực**:
   - **Kp**: Độ nhạy phản hồi vị trí góc.
   - **Ki**: Tích phân bù trôi.
   - **Kd**: Đạo hàm triệt tiêu dao động/lắc.
   - **Trim**: Chỉnh góc thăng bằng cơ khí của robot (&plusmn;5.0&deg;).
   - Tự động gửi lệnh cập nhật ngay khi kéo thanh trượt (có checkbox bật/tắt).
4. **Nút Dừng Khẩn Cấp (E-STOP)**:
   - Ngắt ngay thông số PID về 0 để dừng xe khi có sự cố.

