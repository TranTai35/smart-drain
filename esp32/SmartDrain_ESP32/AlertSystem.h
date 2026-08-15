#ifndef ALERT_SYSTEM_H
#define ALERT_SYSTEM_H

// Mức cảnh báo dùng cho LED và buzzer trên board
enum AlertLevel
{
    ALERT_NORMAL,
    ALERT_HIGH,
    ALERT_DANGER
};

extern AlertLevel currentAlertLevel;

void setupAlertSystem();
void updateAlertSystem();

void setBuzzerMuted(bool muted);
bool isBuzzerMuted();

const char* getAlertLevelName();

// =========================
// THÔNG TIN CẢNH BÁO
// =========================
//
// Đây là NGUỒN SỰ THẬT DUY NHẤT về cảnh báo của hệ thống.
// LCD, MQTT và website đều lấy từ đây, nên không thể xảy ra trường hợp
// màn hình hiển thị một đằng còn website báo một nẻo.
//
// Thứ tự ưu tiên xem determineAlertLevel() trong AlertSystem.cpp

// Mã cảnh báo gửi lên MQTT, ví dụ "DRAIN_ABNORMAL"
const char* getAlertCode();

// Mức độ theo bảng mục 5 của docs/mqtt-protocol.md: INFO / WARNING / DANGER
const char* getAlertSeverity();

// Mô tả đầy đủ, không dấu, dùng cho MQTT và website
const char* getAlertMessage();

// Bản rút gọn tối đa 16 ký tự, dùng cho LCD 1602
const char* getAlertShortText();

bool isAlertActive();

#endif
