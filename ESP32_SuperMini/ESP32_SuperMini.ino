/**
 * @file ESP32_SuperMini.ino
 * @brief Firmware cho ESP32-S3 Super Mini:
 *        - Đọc dữ liệu Telemetry từ STM32F412 (PA9 TX, PA10 RX) qua UART
 *        - Phát Web Server trên mạng Local (WiFi STA) và điểm phát WiFi AP
 *        - Hiển thị góc MPU6050, đồ thị, trạng thái robot
 *        - Điều chỉnh thông số PID và gửi ngược lại về STM32 thời gian thực
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "config.h"
#include "web_index.h"

// ──────────────────────────────────────────────
// Khởi tạo WebServer cổng 80
// ──────────────────────────────────────────────
WebServer server(80);

// Struct lưu trữ telemetry nhận từ STM32
struct TelemetryData {
  float angle = 0.0f;
  float rate = 0.0f;
  float acc_angle = 0.0f;
  float v_cmd = 0.0f;
  float a_cmd = 0.0f;
  int   state = 0;
  float kp = 600.0f;
  float ki = 0.0f;
  float kd = 20.0f;
  float trim = 0.0f;
  int   fault = 0;
  int   dt_us = 0;
  uint32_t last_update_ms = 0;
} telem;

// Buffer nhận UART
String rxBuffer = "";

// ──────────────────────────────────────────────
// Xử lý frame telemetry từ STM32:
// $T,angle,rate,acc_angle,v_cmd,a_cmd,state,Kp,Ki,Kd,trim,fault,dt_us\n
// ──────────────────────────────────────────────
void parseTelemetryFrame(const String& frame) {
  if (!frame.startsWith("$T,")) return;

  // Tách các trường theo dấu phẩy
  int startIdx = 3;
  int fieldIndex = 0;
  String fields[13];

  for (int i = 3; i < frame.length(); i++) {
    if (frame[i] == ',' || frame[i] == '\n' || frame[i] == '\r') {
      fields[fieldIndex++] = frame.substring(startIdx, i);
      startIdx = i + 1;
      if (fieldIndex >= 12) break;
    }
  }

  if (fieldIndex >= 11) {
    telem.angle      = fields[0].toFloat();
    telem.rate       = fields[1].toFloat();
    telem.acc_angle  = fields[2].toFloat();
    telem.v_cmd      = fields[3].toFloat();
    telem.a_cmd      = fields[4].toFloat();
    telem.state      = fields[5].toInt();
    telem.kp         = fields[6].toFloat();
    telem.ki         = fields[7].toFloat();
    telem.kd         = fields[8].toFloat();
    telem.trim       = fields[9].toFloat();
    telem.fault      = fields[10].toInt();
    if (fieldIndex >= 12) {
      telem.dt_us    = fields[11].toInt();
    }
    telem.last_update_ms = millis();
  }
}

// ──────────────────────────────────────────────
// Gửi lệnh PID sang STM32:
// $P,Kp,Ki,Kd,trim\n
// ──────────────────────────────────────────────
void sendPidToStm32(float kp, float ki, float kd, float trim) {
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "$P,%.2f,%.2f,%.2f,%.2f\n", kp, ki, kd, trim);
  Serial1.print(cmd);
  Serial.printf("[TX to STM32]: %s", cmd);
}

// ──────────────────────────────────────────────
// Các Endpoint HTTP
// ──────────────────────────────────────────────
void handleRoot() {
  server.send_P(200, "text/html", PAGE_MAIN);
}

void handleGetData() {
  // Trả về JSON cho web app polling
  char json[256];
  snprintf(json, sizeof(json),
    "{\"angle\":%.2f,\"rate\":%.2f,\"acc_angle\":%.2f,"
    "\"v_cmd\":%.1f,\"a_cmd\":%.1f,\"state\":%d,"
    "\"kp\":%.2f,\"ki\":%.2f,\"kd\":%.2f,\"trim\":%.2f,"
    "\"fault\":%d,\"dt_us\":%d,\"age_ms\":%lu}",
    telem.angle, telem.rate, telem.acc_angle,
    telem.v_cmd, telem.a_cmd, telem.state,
    telem.kp, telem.ki, telem.kd, telem.trim,
    telem.fault, telem.dt_us,
    (unsigned long)(millis() - telem.last_update_ms)
  );

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleSetPid() {
  if (server.hasArg("kp") && server.hasArg("kd")) {
    float kp   = server.arg("kp").toFloat();
    float ki   = server.arg("ki").toFloat();
    float kd   = server.arg("kd").toFloat();
    float trim = server.arg("trim").toFloat();

    // Gửi ngay lập tức sang STM32
    sendPidToStm32(kp, ki, kd, trim);

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request: Missing parameters");
  }
}

// ──────────────────────────────────────────────
// SETUP
// ──────────────────────────────────────────────
void setup() {
  // Serial USB cho Debug
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=================================");
  Serial.println("  ESP32-S3 Robot Telemetry Bridge ");
  Serial.println("=================================");

  // Khởi tạo UART1 nối với STM32F412 (PA9 TX, PA10 RX)
  Serial1.begin(STM32_BAUDRATE, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
  Serial.printf("UART1 khoi tao: RX=GPIO%d, TX=GPIO%d @ %d baud\n",
                STM32_RX_PIN, STM32_TX_PIN, STM32_BAUDRATE);

  // Cấu hình đồng thời WiFi STA (mạng nhà) và WiFi AP (điểm phát phụ)
  WiFi.mode(WIFI_AP_STA);

  // Bật WiFi AP trước
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
  Serial.printf("WiFi AP bat: SSID '%s' (Pass: '%s')\n", WIFI_AP_SSID, WIFI_AP_PASS);
  Serial.print("Dia chi IP AP: http://");
  Serial.println(WiFi.softAPIP());

  // Kết nối mạng Local WiFi nhà bạn
  if (String(WIFI_STA_SSID) != "YOUR_WIFI_SSID") {
    Serial.printf("Dang ket noi WiFi Local '%s'...\n", WIFI_STA_SSID);
    WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);

    int timeout = 20; // 10 giây
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
      delay(500);
      Serial.print(".");
      timeout--;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Da ket noi WiFi Local! Dia chi IP: http://");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Khong the ket noi WiFi Local. Vui long dung mang AP cua robot!");
    }
  }

  // Khởi tạo mDNS: truy cập qua http://robot.local
  if (MDNS.begin(MDNS_NAME)) {
    Serial.printf("mDNS hoat dong: http://%s.local\n", MDNS_NAME);
  }

  // Cấu hình các routing cho Web Server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleGetData);
  server.on("/api/pid", HTTP_POST, handleSetPid);

  server.begin();
  Serial.println("Web Server da san sang phuc vu!");
}

// ──────────────────────────────────────────────
// LOOP
// ──────────────────────────────────────────────
void loop() {
  server.handleClient();

  // Đọc dữ liệu từ STM32 qua UART1
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();
    if (c == '$') {
      rxBuffer = "$";
    } else if (c == '\n') {
      rxBuffer += c;
      parseTelemetryFrame(rxBuffer);
      rxBuffer = "";
    } else if (rxBuffer.length() > 0) {
      rxBuffer += c;
      if (rxBuffer.length() > 160) {
        rxBuffer = ""; // Tránh tràn bộ nhớ nếu mất đồng bộ
      }
    }
  }
}

