#include <Arduino.h>
#include <string.h>

#include "Config.h"
#include "RuntimeConfig.h"

// ========================================
// GIÁ TRỊ CẤU HÌNH ĐANG ĐƯỢC SỬ DỤNG
// ========================================

int pumpStartLevel = PUMP_START_LEVEL;
int pumpStopLevel = PUMP_STOP_LEVEL;
int outputLimit = OUTPUT_DANGER_LEVEL;

unsigned long maxPumpRuntimeMs =
    MAX_PUMP_RUNTIME_MS;

unsigned long drainCheckIntervalMs =
    DRAIN_CHECK_INTERVAL_MS;

int drainMinDropPercent =
    DRAIN_MIN_DROP_PERCENT;

// ========================================
// KHÔI PHỤC CẤU HÌNH MẶC ĐỊNH
// ========================================

void resetRuntimeConfig()
{
    pumpStartLevel = PUMP_START_LEVEL;
    pumpStopLevel = PUMP_STOP_LEVEL;
    outputLimit = OUTPUT_DANGER_LEVEL;

    maxPumpRuntimeMs =
        MAX_PUMP_RUNTIME_MS;

    drainCheckIntervalMs =
        DRAIN_CHECK_INTERVAL_MS;

    drainMinDropPercent =
        DRAIN_MIN_DROP_PERCENT;

    Serial.println(
        "[CONFIG] Da khoi phuc cau hinh mac dinh"
    );
}

// ========================================
// ÁP DỤNG MỘT THÔNG SỐ
// ========================================

bool applyRuntimeConfig(
    const char* key,
    long value)
{
    if (key == nullptr)
    {
        Serial.println(
            "[CONFIG] Tu khoa khong hop le"
        );

        return false;
    }

    // ------------------------------------
    // Ngưỡng bật bơm: pump_start
    // ------------------------------------

    if (strcmp(key, "pump_start") == 0)
    {
        // Phải lớn hơn ngưỡng tắt và không quá 100%
        if (value <= pumpStopLevel ||
            value > 100)
        {
            Serial.println(
                "[CONFIG] pump_start khong hop le"
            );

            return false;
        }

        pumpStartLevel = (int)value;

        Serial.print(
            "[CONFIG] pump_start = "
        );
        Serial.println(pumpStartLevel);

        return true;
    }

    // ------------------------------------
    // Ngưỡng tắt bơm: pump_stop
    // ------------------------------------

    if (strcmp(key, "pump_stop") == 0)
    {
        // Phải từ 0% và nhỏ hơn ngưỡng bật
        if (value < 0 ||
            value >= pumpStartLevel)
        {
            Serial.println(
                "[CONFIG] pump_stop khong hop le"
            );

            return false;
        }

        pumpStopLevel = (int)value;

        Serial.print(
            "[CONFIG] pump_stop = "
        );
        Serial.println(pumpStopLevel);

        return true;
    }

    // ------------------------------------
    // Giới hạn bể xả: output_limit
    // ------------------------------------

    if (strcmp(key, "output_limit") == 0)
    {
        if (value <= 0 ||
            value > 100)
        {
            Serial.println(
                "[CONFIG] output_limit khong hop le"
            );

            return false;
        }

        outputLimit = (int)value;

        Serial.print(
            "[CONFIG] output_limit = "
        );
        Serial.println(outputLimit);

        return true;
    }

    // ------------------------------------
    // Thời gian bơm tối đa, đơn vị giây
    // max_runtime
    // ------------------------------------

    if (strcmp(key, "max_runtime") == 0)
    {
        // Cho phép từ 10 đến 600 giây
        if (value < 10 ||
            value > 600)
        {
            Serial.println(
                "[CONFIG] max_runtime khong hop le"
            );

            return false;
        }

        maxPumpRuntimeMs =
            (unsigned long)value * 1000UL;

        Serial.print(
            "[CONFIG] max_runtime = "
        );
        Serial.print(
            maxPumpRuntimeMs / 1000UL
        );
        Serial.println(" s");

        return true;
    }

    // ------------------------------------
    // Chu kỳ kiểm tra thoát nước, giây
    // drain_check
    // ------------------------------------

    if (strcmp(key, "drain_check") == 0)
    {
        // Giới hạn an toàn từ 5 đến 300 giây
        if (value < 5 ||
            value > 300)
        {
            Serial.println(
                "[CONFIG] drain_check khong hop le"
            );

            return false;
        }

        drainCheckIntervalMs =
            (unsigned long)value * 1000UL;

        Serial.print(
            "[CONFIG] drain_check = "
        );
        Serial.print(
            drainCheckIntervalMs / 1000UL
        );
        Serial.println(" s");

        return true;
    }

    // ------------------------------------
    // Mức giảm tối thiểu, đơn vị %
    // drain_min_drop
    // ------------------------------------

    if (strcmp(key, "drain_min_drop") == 0)
    {
        if (value < 1 ||
            value > 100)
        {
            Serial.println(
                "[CONFIG] drain_min_drop khong hop le"
            );

            return false;
        }

        drainMinDropPercent =
            (int)value;

        Serial.print(
            "[CONFIG] drain_min_drop = "
        );
        Serial.print(drainMinDropPercent);
        Serial.println("%");

        return true;
    }

    // ------------------------------------
    // Không nhận diện được từ khóa
    // ------------------------------------

    Serial.print(
        "[CONFIG] Khong ho tro key: "
    );
    Serial.println(key);

    return false;
}