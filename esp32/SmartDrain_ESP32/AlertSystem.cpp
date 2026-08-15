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
    // DANGER được ưu tiên trước WARNING
    if (outputWaterPercent >=
        outputLimit)
    {
        currentInternalAlert =
            INTERNAL_ALERT_OUTPUT_FULL;

        currentAlertLevel =
            ALERT_DANGER;

        return;
    }

    if (inputWaterPercent >= 90)
    {
        currentInternalAlert =
            INTERNAL_ALERT_INPUT_DANGER;

        currentAlertLevel =
            ALERT_DANGER;

        return;
    }

    if (pumpFault ==
        PUMP_FAULT_TIMEOUT)
    {
        currentInternalAlert =
            INTERNAL_ALERT_PUMP_TIMEOUT;

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