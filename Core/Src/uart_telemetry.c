/**
 * @file uart_telemetry.c
 * @brief Truyền telemetry và nhận lệnh PID qua USART1 (PA9 TX, PA10 RX).
 *
 * Frame gửi (STM32 → ESP32), ~20 Hz:
 *   $T,angle,rate,acc_angle,v_cmd,a_cmd,state,Kp,Ki,Kd,trim,fault,dt_us\n
 *
 * Frame nhận (ESP32 → STM32):
 *   $P,Kp,Ki,Kd,trim\n
 *
 * Hỗ trợ cả 2 chế độ:
 *   1. Trực tiếp qua thanh ghi CMSIS USART1 (hoạt động ngay cả khi chưa bật HAL UART trong CubeMX)
 *   2. Qua HAL UART khi đã cấu hình USART1 trong CubeMX
 */
#include "uart_telemetry.h"
#include "debug_vars.h"
#include "app_config.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* Khai báo weak cho huart1 để không bị lỗi unresolved symbol nếu CubeMX chưa sinh */
__attribute__((weak)) UART_HandleTypeDef huart1 = {0};

/* ──────── Tham số ──────── */
#define UART_TX_TIMEOUT_MS   5U    /* Timeout TX ngắn để không chặn vòng lặp */
#define RX_BUF_SIZE          96U   /* Độ dài tối đa 1 frame nhận */

/* ──────── Trạng thái nội bộ ──────── */
static UART_HandleTypeDef *s_huart __attribute__((unused)) = NULL;
static bool s_use_hal __attribute__((unused)) = false;

/* Buffer nhận từng byte */
static uint8_t  s_rx_byte = 0;           /* Buffer 1 byte cho interrupt HAL */
static char     s_rx_buf[RX_BUF_SIZE];   /* Tích lũy frame */
static uint16_t s_rx_idx = 0;

/* ──────── Biến ngoài (định nghĩa trong app_main.c) ──────── */
extern app_debug_t g_dbg;
extern pid_gains_t g_gains;

/* ──────────────────────────────────────────────
   Helper: chuyển float → 2 phần int (tránh float printf)
   ────────────────────────────────────────────── */
typedef struct { int32_t whole; int32_t frac; uint8_t neg; } FloatParts;

static FloatParts split_float(float v, int decimals)
{
    FloatParts p;
    p.neg = (v < 0.0f) ? 1 : 0;
    if (p.neg) v = -v;

    /* Nhân 10^decimals */
    float scale = 1.0f;
    for (int i = 0; i < decimals; i++) scale *= 10.0f;

    p.whole = (int32_t)v;
    p.frac  = (int32_t)((v - (float)p.whole) * scale + 0.5f);

    /* Tràn phần thập phân */
    int32_t limit = (int32_t)scale;
    if (p.frac >= limit) { p.whole++; p.frac = 0; }
    return p;
}

/* Ghi float vào buffer text, trả về số byte đã ghi */
static int write_float(char *buf, float v, int dec)
{
    FloatParts p = split_float(v, dec);
    int n = 0;
    if (p.neg) buf[n++] = '-';

    /* Phần nguyên */
    int wlen = 0;
    char tmp[12];
    int32_t w = p.whole;
    if (w == 0) { tmp[wlen++] = '0'; }
    else { while (w > 0) { tmp[wlen++] = (char)('0' + w % 10); w /= 10; } }
    for (int i = wlen - 1; i >= 0; i--) buf[n++] = tmp[i];

    /* Phần thập phân */
    buf[n++] = '.';
    char fstr[8];
    int flen = 0;
    int32_t f = p.frac;
    /* Đảm bảo đủ số chữ số */
    for (int i = 0; i < dec; i++) fstr[flen++] = '0';
    int pos = dec - 1;
    while (f > 0 && pos >= 0) {
        fstr[pos--] = (char)('0' + f % 10);
        f /= 10;
    }
    for (int i = 0; i < dec; i++) buf[n++] = fstr[i];

    return n;
}

/* ──────── API công khai ──────── */

void uart_telemetry_init(UART_HandleTypeDef *huart)
{
    s_rx_idx = 0;
    memset(s_rx_buf, 0, sizeof(s_rx_buf));

#ifdef HAL_UART_MODULE_ENABLED
    if (huart != NULL && huart->Instance != NULL) {
        s_huart = huart;
        s_use_hal = true;
        return;
    }
#else
    (void)huart;
#endif

    /* Tự khởi tạo thanh ghi trực tiếp cho USART1: PA9 (TX), PA10 (RX) @ 115200 baud */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* Cấu hình PA9, PA10: Alternate Function (10b) */
    GPIOA->MODER &= ~(GPIO_MODER_MODER9 | GPIO_MODER_MODER10);
    GPIOA->MODER |= (2U << GPIO_MODER_MODER9_Pos) | (2U << GPIO_MODER_MODER10_Pos);

    /* Tốc độ cao */
    GPIOA->OSPEEDR |= (3U << GPIO_OSPEEDR_OSPEED9_Pos) | (3U << GPIO_OSPEEDR_OSPEED10_Pos);

    /* PA10 RX có pull-up để ổn định khi chưa cắm dây */
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD9 | GPIO_PUPDR_PUPD10);
    GPIOA->PUPDR |= (1U << GPIO_PUPDR_PUPD10_Pos);

    /* AF7 cho PA9 và PA10 (AFR[1] vì pin 9, 10 >= 8) */
    GPIOA->AFR[1] &= ~((0xFU << ((9 - 8) * 4)) | (0xFU << ((10 - 8) * 4)));
    GPIOA->AFR[1] |= (7U << ((9 - 8) * 4)) | (7U << ((10 - 8) * 4));

    /* Cấu hình baudrate 115200 */
    uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
    if (pclk2 == 0) pclk2 = 16000000U;
    USART1->BRR = (pclk2 + 115200U / 2U) / 115200U;

    /* Bật TE (Transmitter), RE (Receiver), RXNEIE (Ngắt nhận), UE (USART Enable) */
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    NVIC_SetPriority(USART1_IRQn, 5);
    NVIC_EnableIRQ(USART1_IRQn);
    s_use_hal = false;
}

void uart_telemetry_start_rx(void)
{
#ifdef HAL_UART_MODULE_ENABLED
    if (s_use_hal && s_huart != NULL && s_huart->Instance != NULL) {
        HAL_UART_Receive_IT(s_huart, &s_rx_byte, 1);
    }
#endif
}

void uart_telemetry_send(void)
{
    /* Xây dựng frame: $T,angle,rate,acc_angle,v_cmd,a_cmd,state,Kp,Ki,Kd,trim,fault,dt_us\n */
    char buf[160];
    int  n = 0;

    buf[n++] = '$'; buf[n++] = 'T'; buf[n++] = ',';

    /* angle_deg */
    n += write_float(buf + n, g_dbg.angle_deg,     2); buf[n++] = ',';
    /* rate_dps */
    n += write_float(buf + n, g_dbg.rate_dps,      2); buf[n++] = ',';
    /* acc_angle_deg */
    n += write_float(buf + n, g_dbg.acc_angle_deg, 2); buf[n++] = ',';
    /* v_cmd */
    n += write_float(buf + n, g_dbg.v_cmd,         1); buf[n++] = ',';
    /* a_cmd */
    n += write_float(buf + n, g_dbg.a_cmd,         1); buf[n++] = ',';

    /* state (int) */
    int32_t st = g_dbg.state;
    buf[n++] = (char)('0' + (st < 0 ? 0 : (st > 9 ? 9 : st)));
    buf[n++] = ',';

    /* Kp, Ki, Kd, trim */
    n += write_float(buf + n, g_gains.Kp,   2); buf[n++] = ',';
    n += write_float(buf + n, g_gains.Ki,   2); buf[n++] = ',';
    n += write_float(buf + n, g_gains.Kd,   2); buf[n++] = ',';
    n += write_float(buf + n, g_gains.trim, 2); buf[n++] = ',';

    /* fault_flags (uint32, hex 1 digit giản lược) */
    uint32_t ff = g_dbg.fault_flags;
    buf[n++] = (char)('0' + (ff > 9 ? 9 : ff)); buf[n++] = ',';

    /* loop_dt_us */
    uint32_t dt = g_dbg.loop_dt_us;
    char dtstr[10]; int dtlen = 0;
    if (dt == 0) { dtstr[dtlen++] = '0'; }
    else { uint32_t tmp = dt; while (tmp > 0) { dtstr[dtlen++] = (char)('0' + tmp % 10); tmp /= 10; } }
    for (int i = dtlen - 1; i >= 0; i--) buf[n++] = dtstr[i];

    buf[n++] = '\n';
    buf[n]   = '\0';

#ifdef HAL_UART_MODULE_ENABLED
    if (s_use_hal && s_huart != NULL && s_huart->Instance != NULL) {
        HAL_UART_Transmit(s_huart, (uint8_t *)buf, (uint16_t)n, UART_TX_TIMEOUT_MS);
        return;
    }
#endif

    /* Gửi trực tiếp qua thanh ghi USART1 không blocking */
    for (int i = 0; i < n; i++) {
        uint32_t timeout = 5000;
        while (!(USART1->SR & USART_SR_TXE) && --timeout);
        if (timeout == 0) break;
        USART1->DR = (uint8_t)buf[i];
    }
}

/**
 * @brief Parse frame "$P,Kp,Ki,Kd,trim\n" và cập nhật g_gains.
 */
static void parse_pid_frame(const char *frame)
{
    /* Bỏ qua "$P," ở đầu */
    const char *p = frame + 3;
    char *end;

    float kp = strtof(p, &end); if (*end != ',') return; p = end + 1;
    float ki = strtof(p, &end); if (*end != ',') return; p = end + 1;
    float kd = strtof(p, &end); if (*end != ',') return; p = end + 1;
    float tr = strtof(p, &end);

    /* Giới hạn an toàn */
    if (kp < 0.0f || kp > 5000.0f) return;
    if (ki < 0.0f || ki > 100.0f)  return;
    if (kd < 0.0f || kd > 200.0f)  return;
    if (tr < -10.0f || tr > 10.0f) return;

    /* Cập nhật nguyên tử */
    g_gains.Kp   = kp;
    g_gains.Ki   = ki;
    g_gains.Kd   = kd;
    g_gains.trim = tr;
}

void uart_telemetry_rx_byte(uint8_t byte)
{
    if (byte == '$') {
        s_rx_idx = 0;
    }

    if (s_rx_idx < RX_BUF_SIZE - 1) {
        s_rx_buf[s_rx_idx++] = (char)byte;
        s_rx_buf[s_rx_idx]   = '\0';
    } else {
        s_rx_idx = 0;
        return;
    }

    if (byte == '\n') {
        if (s_rx_idx >= 4 &&
            s_rx_buf[0] == '$' &&
            s_rx_buf[1] == 'P' &&
            s_rx_buf[2] == ',') {
            parse_pid_frame(s_rx_buf);
        }
        s_rx_idx = 0;
    }
}

/**
 * @brief Gọi từ HAL_UART_RxCpltCallback: xử lý byte đã nhận và bật lại IT.
 */
void uart_telemetry_on_rx_complete(void)
{
    uart_telemetry_rx_byte(s_rx_byte);
    uart_telemetry_start_rx();
}
