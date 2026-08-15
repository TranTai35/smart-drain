#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H

#include <Arduino.h>

// Các thông số thực tế đang được hệ thống sử dụng
extern int pumpStartLevel;
extern int pumpStopLevel;
extern int outputLimit;

extern unsigned long maxPumpRuntimeMs;
extern unsigned long drainCheckIntervalMs;

extern int drainMinDropPercent;

// Đưa tất cả thông số về giá trị mặc định trong Config.h
void resetRuntimeConfig();

// Áp dụng lệnh KEY=VALUE nhận từ MQTT
// Trả về true nếu giá trị hợp lệ
bool applyRuntimeConfig(
    const char* key,
    long value
);

#endif