#include <Arduino.h>

#include "Config.h"
#include "RuntimeConfig.h"
#include "WaterSensor.h"
#include "AlertSystem.h"
#include "Pump.h"

// =========================
// INTERNAL ALERT CODE
// =========================

enum InternalAlertCode
{
    INTERNAL_ALERT_NONE,
    INTERNAL_ALERT_INPUT_DANGER,
    INTERNAL_ALERT_OUTPUT_FULL,
    INTERNAL_ALERT_PUMP_TIMEOUT,
    INTERNAL_ALERT_DRAIN_ABNORMAL
};

// =========================
// CURRENT STATE
// =========================

AlertLevel currentAlertLevel =
    ALERT_NORMAL;

AlertLevel previousAlertLevel =
    ALERT_NORMAL;

InternalAlertCode currentInternalAlert =
    INTERNAL_ALERT_NONE;

InternalAlertCode previousInternalAlert =
    INTERNAL_ALERT_NONE;

bool buzzerMuted = false;

// =========================
// SETTINGS
// =========================

const unsigned long
    DANGER_BLINK_INTERVAL = 250UL;

// =========================
// SETUP
// =========================

void setupAlertSystem()
{
    pinMode(
        ALERT_LED_PIN,
        OUTPUT
    );

    pinMode(
        BUZZER_PIN,
        OUTPUT
    );

    digitalWrite(
        ALERT_LED_PIN,
        LOW
    );

    digitalWrite(
        BUZZER_PIN,
        BUZZER_OFF
    );

    currentAlertLevel =
        ALERT_NORMAL;

    previousAlertLevel =
        ALERT_NORMAL;

    currentInternalAlert =
        INTERNAL_ALERT_NONE;

    previousInternalAlert =
        INTERNAL_ALERT_NONE;

    buzzerMuted = false;
}

// =========================
// DETERMINE ALERT
// =========================

void determineAlertLevel()
{
    // ============================================================
    // THỨ TỰ ƯU TIÊN CẢNH BÁO
    // ============================================================
    //
    // Hai lỗi chốt (PUMP_TIMEOUT, DRAIN_ABNORMAL) phải đứng TRÊN
    // INPUT_TANK_DANGER, nếu không chúng gần như không bao giờ hiện được.
    //
    // Lý do: cảnh báo thoát nước bất thường sinh ra để phát hiện ống nghẹt.
    // Nhưng khi ống nghẹt thì nước không thoát, bể thu tiếp tục dâng và vượt
    // 90%. Nếu INPUT_TANK_DANGER đứng trên, nó sẽ đè mất DRAIN_ABNORMAL, và
    // người dùng chỉ thấy "bể thu nguy hiểm" mà không biết ống đang nghẹt.
    // PUMP_TIMEOUT bị che tương tự vì bơm quá giờ thường xảy ra lúc nước nhiều.
    //
    // Riêng OUTPUT_TANK_FULL vẫn đứng đầu: nước sắp tràn ra ngoài là tình
    // huống nguy hiểm nhất, phải báo trước mọi thứ khác.
    //
    // Thứ tự này khớp bảng mục 5 của docs/mqtt-protocol.md.

    if (outputWaterPercent >=
        outputLimit)
    {
        currentInternalAlert =
            INTERNAL_ALERT_OUTPUT_FULL;

        currentAlertLevel =
            ALERT_DANGER;

        return;
    }

    if (pumpFault ==
        PUMP_FAULT_TIMEOUT)
    {
        currentInternalAlert =
            INTERNAL_ALERT_PUMP_TIMEOUT;

        // Gửi lên website là WARNING theo bảng mục 5, nhưng LED và buzzer
        // trên board vẫn báo ở mức DANGER: mục 3.1 báo cáo yêu cầu buzzer
        // kêu khi "có lỗi vận hành", và người đứng cạnh mô hình cần biết ngay.
        currentAlertLevel =
            ALERT_DANGER;

        return;
    }

    if (pumpFault ==
        PUMP_FAULT_DRAIN_ABNORMAL)
    {
        currentInternalAlert =
            INTERNAL_ALERT_DRAIN_ABNORMAL;

        currentAlertLevel =
            ALERT_DANGER;

        return;
    }

    if (inputWaterPercent >= INPUT_DANGER_LEVEL)
    {
        currentInternalAlert =
            INTERNAL_ALERT_INPUT_DANGER;

        currentAlertLevel =
            ALERT_DANGER;

        return;
    }

    currentInternalAlert =
        INTERNAL_ALERT_NONE;

    // HIGH chỉ bật LED, không phát buzzer
    if (inputWaterPercent >= 70 ||
        outputWaterPercent >= 70)
    {
        currentAlertLevel =
            ALERT_HIGH;
    }
    else
    {
        currentAlertLevel =
            ALERT_NORMAL;
    }
}

// =========================
// NEW ALERT
// =========================

void checkForNewAlert()
{
    if (currentInternalAlert ==
        previousInternalAlert)
    {
        return;
    }

    // Khi xuất hiện một cảnh báo mới,
    // buzzer tự bỏ MUTE theo giao thức MQTT
    if (currentInternalAlert !=
        INTERNAL_ALERT_NONE)
    {
        buzzerMuted = false;

        Serial.println(
            "[BUZZER] Canh bao moi - "
            "tu dong UNMUTE"
        );
    }

    previousInternalAlert =
        currentInternalAlert;
}

// =========================
// UPDATE OUTPUTS
// =========================

void updateAlertOutputs()
{
    switch (currentAlertLevel)
    {
        case ALERT_NORMAL:
        {
            digitalWrite(
                ALERT_LED_PIN,
                LOW
            );

            digitalWrite(
                BUZZER_PIN,
                BUZZER_OFF
            );

            break;
        }

        case ALERT_HIGH:
        {
            // Mức HIGH:
            // LED sáng liên tục
            digitalWrite(
                ALERT_LED_PIN,
                HIGH
            );

            digitalWrite(
                BUZZER_PIN,
                BUZZER_OFF
            );

            break;
        }

        case ALERT_DANGER:
        {
            bool blinkState =
                (
                    millis() /
                    DANGER_BLINK_INTERVAL
                ) % 2 == 0;

            // LED luôn nhấp nháy
            // dù buzzer đang bị MUTE
            digitalWrite(
                ALERT_LED_PIN,
                blinkState
                    ? HIGH
                    : LOW
            );

            if (buzzerMuted)
            {
                digitalWrite(
                    BUZZER_PIN,
                    BUZZER_OFF
                );
            }
            else
            {
                digitalWrite(
                    BUZZER_PIN,
                    blinkState
                        ? BUZZER_ON
                        : BUZZER_OFF
                );
            }

            break;
        }
    }
}

// =========================
// MAIN UPDATE
// =========================

void updateAlertSystem()
{
    determineAlertLevel();
    checkForNewAlert();
    updateAlertOutputs();

    if (currentAlertLevel !=
        previousAlertLevel)
    {
        previousAlertLevel =
            currentAlertLevel;

        Serial.print("[ALERT] ");

        Serial.println(
            getAlertLevelName()
        );
    }
}

// =========================
// BUZZER MUTE
// =========================

void setBuzzerMuted(bool muted)
{
    if (buzzerMuted == muted)
    {
        return;
    }

    buzzerMuted = muted;

    if (buzzerMuted)
    {
        // Tắt buzzer ngay khi nhận MUTE
        digitalWrite(
            BUZZER_PIN,
            BUZZER_OFF
        );

        Serial.println(
            "[BUZZER] MUTED"
        );
    }
    else
    {
        Serial.println(
            "[BUZZER] UNMUTED"
        );
    }
}

bool isBuzzerMuted()
{
    return buzzerMuted;
}

// =========================
// DISPLAY NAME
// =========================

const char* getAlertLevelName()
{
    switch (currentAlertLevel)
    {
        case ALERT_HIGH:
            return "HIGH";

        case ALERT_DANGER:
            return "DANGER";

        default:
            return "NORMAL";
    }
}

// =========================
// THÔNG TIN CẢNH BÁO
// =========================
//
// Bốn hàm dưới đây đều suy ra từ cùng một biến currentInternalAlert, nên
// mã, mức độ và mô tả không bao giờ mâu thuẫn với nhau.

const char* getAlertCode()
{
    switch (currentInternalAlert)
    {
        case INTERNAL_ALERT_OUTPUT_FULL:
            return "OUTPUT_TANK_FULL";

        case INTERNAL_ALERT_PUMP_TIMEOUT:
            return "PUMP_TIMEOUT";

        case INTERNAL_ALERT_DRAIN_ABNORMAL:
            return "DRAIN_ABNORMAL";

        case INTERNAL_ALERT_INPUT_DANGER:
            return "INPUT_TANK_DANGER";

        default:
            return "NONE";
    }
}

const char* getAlertSeverity()
{
    switch (currentInternalAlert)
    {
        case INTERNAL_ALERT_OUTPUT_FULL:
        case INTERNAL_ALERT_INPUT_DANGER:
            return "DANGER";

        case INTERNAL_ALERT_PUMP_TIMEOUT:
        case INTERNAL_ALERT_DRAIN_ABNORMAL:
            return "WARNING";

        default:
            return "INFO";
    }
}

const char* getAlertMessage()
{
    switch (currentInternalAlert)
    {
        case INTERNAL_ALERT_OUTPUT_FULL:
            return "Be xa da day, da dung bom";

        case INTERNAL_ALERT_PUMP_TIMEOUT:
            return "Bom chay qua thoi gian cho phep";

        case INTERNAL_ALERT_DRAIN_ABNORMAL:
            return "Thoat nuoc bat thuong, kiem tra ong";

        case INTERNAL_ALERT_INPUT_DANGER:
            return "Be thu o muc nguy hiem";

        default:
            return "He thong binh thuong";
    }
}

// Bản rút gọn cho LCD 1602: tối đa 16 ký tự, không dấu
const char* getAlertShortText()
{
    switch (currentInternalAlert)
    {
        case INTERNAL_ALERT_OUTPUT_FULL:
            return "Be xa da day";

        case INTERNAL_ALERT_PUMP_TIMEOUT:
            return "Bom qua gio";

        case INTERNAL_ALERT_DRAIN_ABNORMAL:
            return "Ong bi nghet?";

        case INTERNAL_ALERT_INPUT_DANGER:
            return "Be thu nguy hiem";

        default:
            return "Binh thuong";
    }
}

bool isAlertActive()
{
    return currentInternalAlert !=
        INTERNAL_ALERT_NONE;
}