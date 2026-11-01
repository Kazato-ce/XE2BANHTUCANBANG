# AGENTS.md — Firmware robot tự cân bằng 2 bánh (STM32F412 + 2×TMC2208 + MPU6050)

> File này là đặc tả để Codex viết code và xây dựng chương trình. Đọc toàn bộ trước khi làm.
> Ngôn ngữ: mô tả bằng tiếng Việt; tên file, hàm, biến, macro bằng tiếng Anh; comment trong code bằng tiếng Việt, ngắn gọn.

## 1. Mục tiêu

Viết firmware C (STM32 HAL, dự án sinh từ STM32CubeMX) cho robot tự cân bằng 2 bánh:

- Đọc góc nghiêng từ MPU6050 qua I2C1, lọc bằng complementary filter.
- Bộ điều khiển PD (có Ki tùy chọn) trên góc nghiêng, đầu ra là gia tốc bánh xe.
- Tạo xung STEP cho 2 driver TMC2208 (chế độ STEP/DIR) bằng ngắt timer, tốc độ thay đổi liên tục.
- Có máy trạng thái an toàn: tự tắt động cơ khi robot ngã hoặc cảm biến lỗi.

## 2. Phần cứng

MCU: STM32F412 (Cortex-M4F, có FPU). Xung nhịp mục tiêu: HCLK 100 MHz, APB1 = HCLK/2.

| Chân | Nhãn (User Label) | Chức năng | Ghi chú |
|---|---|---|---|
| PA0 | M1_EN | TMC2208 #1 ENABLE | **Tích cực mức thấp** (Low = bật driver) |
| PA1 | M1_DIR | TMC2208 #1 DIR | |
| PA2 | M1_STEP | TMC2208 #1 STEP | Cạnh lên = 1 microstep |
| PA3 | M2_EN | TMC2208 #2 ENABLE | Tích cực mức thấp |
| PA4 | M2_DIR | TMC2208 #2 DIR | |
| PA5 | M2_STEP | TMC2208 #2 STEP | Cạnh lên = 1 microstep |
| PB3 | MPU_INT | MPU6050 INT | EXTI3, cạnh lên, INT tích cực mức cao |
| PB6 | I2C1_SCL | MPU6050 SCL | I2C1, Fast Mode 400 kHz |
| PB7 | I2C1_SDA | MPU6050 SDA | I2C1, Fast Mode 400 kHz |

- MPU6050: địa chỉ 7-bit `0x68` (AD0 = GND), HAL dùng `0x68 << 1`.
- Mặc định coi motor 1 là bánh trái, motor 2 là bánh phải (đổi được trong `app_config.h`).
- TMC2208 chạy chế độ standalone STEP/DIR, **không** dùng UART của TMC2208. Vi bước đặt bằng chân MS1/MS2 phần cứng.

## 3. Điều kiện đầu vào (người dùng đã làm trong CubeMX)

Codex **không được sửa file `.ioc`** và không tự sinh lại code CubeMX. Kiểm tra các mục dưới đây có trong dự án; nếu thiếu, **dừng và liệt kê chính xác** mục nào thiếu cùng cách bật trong CubeMX, không tự sửa.

- 6 chân PA0..PA5: `GPIO_Output`, push-pull, no pull. EN khởi tạo mức **High**, DIR/STEP khởi tạo **Low**. STEP: speed Very High.
- PB3: `GPIO_EXTI3`, Rising edge, no pull; bật NVIC `EXTI line3 interrupt`.
- I2C1 trên PB6/PB7, Fast Mode 400 kHz.
- SYS → Debug = `Serial Wire` (để PB3 dùng được làm GPIO).
- **TIM3**: Clock Source = Internal Clock, `Prescaler = 99`, `Counter Period (ARR) = 49`, bật NVIC `TIM3 global interrupt`. Với timer clock 100 MHz, tick = 20 kHz.
- Clock: HCLK 100 MHz, APB1 prescaler /2 (timer APB1 chạy 100 MHz).
- Float: hard FPU (`-mfloat-abi=hard -mfpu=fpv4-sp-d16`).

## 4. Quy tắc bắt buộc

1. Code ứng dụng đặt trong thư mục riêng `Core/Inc/app/` và `Core/Src/app/`. Chỉ thêm hook tối thiểu vào `main.c`, **bên trong các khối `USER CODE BEGIN/END`**:
   - `#include "app/app_main.h"` (USER CODE Includes)
   - `app_init();` (USER CODE 2)
   - `app_loop();` (trong `while(1)`, USER CODE 3)
2. Tự phát hiện hệ build (CMakeLists.txt / Makefile / dự án IDE) và **thêm các file `.c` mới vào build**. Build phải sạch với `-Wall -Wextra`, không warning mới.
3. Trong ISR: không `HAL_Delay`, không I2C, không `printf`, không `malloc`. Thao tác chân STEP dùng ghi thẳng thanh ghi `BSRR`.
4. Không cấp phát động. Không dùng `printf` với float.
5. Biến chia sẻ giữa ISR và main: `volatile`, ghi/đọc 32-bit căn hàng (nguyên tử trên Cortex-M).
6. Mọi hằng số vật lý/chỉnh định nằm trong `app_config.h` (xem mục 7). Không rải số "ma thuật" trong code.
7. HAL callback (`HAL_GPIO_EXTI_Callback`, `HAL_TIM_PeriodElapsedCallback`): nếu đã tồn tại trong `main.c`, **gộp** vào đó qua USER CODE, không định nghĩa trùng.
8. Nếu không có toolchain `arm-none-eabi-gcc` trong môi trường của bạn: nói rõ, vẫn viết code đầy đủ, và viết unit test chạy trên máy host (gcc) cho phần logic thuần (filter, PID, bộ tích lũy pha).

## 5. Cấu trúc file

| File | Nhiệm vụ |
|---|---|
| `app_config.h` | Mọi tham số cấu hình, hằng số, macro đổi dấu/trục |
| `app_main.c/.h` | `app_init()`, `app_loop()`, các HAL callback, lịch chạy vòng điều khiển |
| `mpu6050.c/.h` | Driver MPU6050 (init, đọc 14 byte, quy đổi đơn vị, hiệu chuẩn bias gyro) |
| `attitude.c/.h` | Complementary filter, trả về `angle_deg` và `rate_dps` |
| `pid.c/.h` | Bộ điều khiển PD/PID, chống bão hòa tích phân |
| `stepper.c/.h` | Tạo xung STEP cho 2 motor, điều khiển EN/DIR |
| `balance.c/.h` | Máy trạng thái, vòng điều khiển cân bằng, an toàn |
| `debug_vars.h` | Struct `volatile` để xem/chỉnh trong debugger |

## 6. Đặc tả module

### 6.1 `mpu6050`

- Kiểm tra `WHO_AM_I` (`0x75`) = `0x68`, sai thì trả lỗi.
- Trình tự init:
  - `PWR_MGMT_1 (0x6B)` = `0x80` (reset), đợi 100 ms, rồi ghi `0x01` (clock PLL theo gyro X).
  - `CONFIG (0x1A)` = `0x03` (DLPF ~42 Hz) → tốc độ gyro nội bộ 1 kHz.
  - `SMPLRT_DIV (0x19)` = `4` → tần số lấy mẫu **200 Hz**.
  - `GYRO_CONFIG (0x1B)` = `0x08` (±500 °/s, 65.5 LSB/(°/s)).
  - `ACCEL_CONFIG (0x1C)` = `0x00` (±2 g, 16384 LSB/g).
  - `INT_PIN_CFG (0x37)` = `0x00` (INT tích cực cao, xung ngắn), `INT_ENABLE (0x38)` = `0x01` (data ready).
- Đọc dữ liệu: `HAL_I2C_Mem_Read` 14 byte từ `0x3B` (ACCEL XYZ, TEMP, GYRO XYZ, big-endian), timeout ngắn (~3 ms). Bản đầu dùng đọc blocking trong `app_loop()`; **không** đọc trong ISR.
- Hiệu chuẩn gyro khi khởi động: lấy trung bình ~500 mẫu (≈2.5 s) lúc robot đứng yên để ra bias; nếu dao động vượt ngưỡng thì báo lỗi và làm lại.
- Ánh xạ trục (trục nào là pitch, dấu) cấu hình bằng macro trong `app_config.h`, mặc định: góc từ accel `atan2f(ax, az)`, tốc độ góc lấy từ gyro Y.
- Phục hồi lỗi: sau 3 lần đọc lỗi liên tiếp, khởi tạo lại I2C1 (kèm nhấp SCL 9 lần để giải phóng bus) rồi init lại MPU6050.

### 6.2 `attitude`

- Đo `dt` thực tế bằng DWT cycle counter (bật `DWT->CYCCNT` trong `app_init()`), kẹp trong khoảng 1–20 ms.
- Khởi tạo góc ban đầu từ accel.
- Complementary filter: `angle = a*(angle + rate*dt) + (1-a)*acc_angle`, với `a = tau/(tau + dt)`, `tau = COMP_FILTER_TAU_S` (mặc định 0.5 s).
- Quy ước: **góc dương = robot nghiêng về phía trước**. Dùng macro `ANGLE_SIGN` để đảo.

### 6.3 `pid` / luật điều khiển

Dùng cấu trúc "góc → gia tốc → tốc độ":

```
e      = angle_deg - (ANGLE_SETPOINT_DEG + trim)
a_cmd  = Kp*e + Kd*rate_dps + Ki*integral(e)     // đơn vị steps/s^2
v_cmd += a_cmd * dt                               // tích phân ra tốc độ (steps/s)
v_cmd  = clamp(v_cmd, -V_MAX_SPS, +V_MAX_SPS)
```

- Đạo hàm lấy trực tiếp từ gyro (`rate_dps`), không vi phân số góc.
- Tích phân `Ki` mặc định 0, có kẹp giới hạn và chống bão hòa (anti-windup).
- Gain là biến `volatile` trong `g_gains`, được áp dụng lại mỗi vòng để người dùng chỉnh trực tiếp trong debugger.
- Quy ước: nghiêng về trước (e > 0) → bánh chạy về trước (v_cmd > 0).

### 6.4 `stepper` (tạo xung STEP)

Dùng bộ tích lũy pha (DDS) trong ngắt TIM3 (20 kHz):

- Mỗi motor có `uint32_t acc`, `uint32_t inc`, hướng `dir`. Với tốc độ mong muốn `sps` (steps/s): `inc = fabsf(sps) * 4294967296.0f / tick_hz`, kẹp `inc <= 2^31` (tức tối đa tick_hz/2 step/s).
- Mỗi tick:
  1. Kéo cả hai chân STEP xuống thấp bằng `BSRR` (xung cao rộng 1 tick = 50 µs, đủ cho TMC2208).
  2. `acc += inc`; nếu tràn (acc < inc sau khi cộng) thì kéo chân STEP tương ứng lên cao.
- `stepper_set_speed(motor, sps)`: đặt `inc` và `dir`; đổi DIR (có tính `MOTOR_x_DIR_INVERT`) khi dấu đổi.
- `stepper_enable(bool)`: ghi EN = Low khi bật, High khi tắt. Khởi động mặc định **tắt**.
- `tick_hz` **không hard-code**: tính lúc init từ clock timer thực tế và `PSC/ARR` của TIM3 (`timer_clk = 2*PCLK1` nếu APB1 prescaler ≠ 1). Nếu lệch so với `STEP_TICK_HZ_EXPECTED` quá 1%, đặt cờ lỗi và không cho bật motor.
- Giới hạn gia tốc ở lớp điều khiển (đã có kẹp `V_MAX_SPS`), không cần profile riêng.

### 6.5 `balance` (máy trạng thái và an toàn)

```
INIT -> CALIBRATE -> IDLE -> BALANCE -> FALLEN -> IDLE
                       ^                  |
                       +----- FAULT <-----+  (lỗi IMU)
```

- `INIT`: init MPU6050, kiểm tra WHO_AM_I. Lỗi → `FAULT`.
- `CALIBRATE`: hiệu chuẩn bias gyro. Motor **tắt**.
- `IDLE`: motor tắt. Khi `|angle - setpoint| < START_WINDOW_DEG` liên tục `START_HOLD_MS` → bật motor, vào `BALANCE`, reset PID và `v_cmd = 0`.
- `BALANCE`: chạy vòng điều khiển ở mỗi mẫu MPU6050 mới (200 Hz, kích bằng cờ từ EXTI3).
- `FALLEN`: khi `|angle| > FALL_ANGLE_DEG` → tắt motor ngay (EN = High, speed = 0), chờ về lại cửa sổ khởi động mới sang `IDLE`.
- `FAULT`: mất mẫu IMU quá `IMU_STALE_MS` (mặc định 20 ms) hoặc lỗi I2C liên tục → tắt motor ngay; cứ 500 ms thử init lại IMU, thành công thì về `CALIBRATE`.
- Không bao giờ bật motor trước khi hiệu chuẩn xong và IMU hợp lệ.

### 6.6 `debug_vars`

Struct `volatile app_debug_t g_dbg` gồm: `angle_deg`, `rate_dps`, `acc_angle_deg`, `a_cmd`, `v_cmd`, `state`, `loop_dt_us`, `i2c_err_count`, `fault_flags`. Struct `volatile pid_gains_t g_gains` (Kp, Ki, Kd, trim). Người dùng xem bằng Live Expressions / Cortex-Debug. Chưa cần UART.

## 7. Tham số cấu hình (`app_config.h`)

Giá trị dưới đây là **mặc định khởi điểm để biên dịch được**, không phải giá trị đã tune. Gain phải chỉnh lại trên robot thật (hoặc lấy từ mô phỏng của người dùng).

| Macro | Mặc định | Ý nghĩa |
|---|---|---|
| `MOTOR_FULL_STEPS_PER_REV` | 200 | Động cơ 1.8°/step |
| `MICROSTEP` | **16 (cần người dùng xác nhận theo MS1/MS2)** | Phải khớp phần cứng TMC2208 |
| `STEP_TICK_HZ_EXPECTED` | 20000 | Tick TIM3 |
| `V_MAX_SPS` | 8000 | Tốc độ tối đa (steps/s), phải ≤ tick/2 |
| `KP_DEFAULT` | 600.0f | steps/s² trên mỗi độ |
| `KD_DEFAULT` | 20.0f | steps/s² trên mỗi °/s |
| `KI_DEFAULT` | 0.0f | |
| `ANGLE_SETPOINT_DEG` | 0.0f | Góc cân bằng cơ khí |
| `COMP_FILTER_TAU_S` | 0.5f | Hằng số thời gian complementary filter |
| `START_WINDOW_DEG` | 3.0f | Cửa sổ cho phép bắt đầu cân bằng |
| `START_HOLD_MS` | 1000 | Thời gian giữ thẳng trước khi chạy |
| `FALL_ANGLE_DEG` | 35.0f | Quá góc này thì tắt motor |
| `IMU_STALE_MS` | 20 | Quá thời gian không có mẫu → FAULT |
| `ANGLE_SIGN` | +1 | Đảo dấu góc |
| `MOTOR_1_DIR_INVERT` | 0 | Đảo hướng motor 1 |
| `MOTOR_2_DIR_INVERT` | 1 | Hai motor lắp đối xứng nên thường phải đảo một bên |

## 8. Các giai đoạn và tiêu chí nghiệm thu

Làm tuần tự, mỗi giai đoạn phải build sạch trước khi sang giai đoạn sau.

- **P0 — Khung dự án**: tạo cấu trúc file, hook vào `main.c`, build sạch. Mọi chân ở trạng thái an toàn (EN = High).
- **P1 — IMU**: init + hiệu chuẩn + vòng đọc 200 Hz. Nghiệm thu: `g_dbg.angle_deg` ổn định, đổi dấu đúng khi nghiêng robot, `loop_dt_us` ≈ 5000.
- **P2 — Stepper**: bật cờ biên dịch `APP_TEST_STEPPER`: mỗi motor lần lượt quay +2000 rồi −2000 steps/s, mỗi chiều 2 s, rồi dừng. Nghiệm thu: hai bánh quay đúng chiều khi chỉnh `MOTOR_x_DIR_INVERT`, không mất bước ở 8000 steps/s khi không tải.
- **P3 — Cân bằng**: ghép máy trạng thái và vòng điều khiển. Nghiệm thu: không bật motor khi chưa hiệu chuẩn; tắt motor trong 1 chu kỳ điều khiển khi quá `FALL_ANGLE_DEG` hoặc mất IMU.
- **P4 — Tùy chọn**: vòng ngoài giữ tốc độ/vị trí để chống trôi; telemetry UART (nếu cần UART, **báo lại** cách bật trong CubeMX, không tự sửa `.ioc`).

## 9. Báo cáo cuối của Codex

Khi xong, liệt kê:

1. Danh sách file đã tạo/sửa (kèm vị trí hook trong `main.c`).
2. Kết quả build (lệnh đã chạy, có warning hay không) hoặc lý do không build được.
3. Các giả định đã đặt và chỗ cần người dùng xác nhận trên phần cứng (vi bước, chiều motor, trục MPU6050, gain khởi điểm).
4. Mục nào của mục 3 (điều kiện CubeMX) còn thiếu, nếu có.
