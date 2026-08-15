#include <Arduino.h>

#include "Config.h"
#include "WaterSensor.h"
#include "AlertSystem.h"
#include "Pump.h"

AlertLevel currentAlertLevel = ALERT_NORMAL;
AlertLevel previousAlertLevel = ALERT_NORMAL;

const unsigned long DANGER_BLINK_INTERVAL = 250;

void setupAlertSystem()
{
    pinMode(ALERT_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(ALERT_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, BUZZER_OFF);

    currentAlertLevel = ALERT_NORMAL;
    previousAlertLevel = ALERT_NORMAL;
}

void determineAlertLevel()
{
    if (pumpFault != PUMP_FAULT_NONE)
    {
        currentAlertLevel = ALERT_DANGER;
        return;
    }
    // Bể xả đạt 80% là nguy hiểm vì phải dừng bơm
    if (inputWaterPercent >= 90 ||
        outputWaterPercent >= 80)
    {
        currentAlertLevel = ALERT_DANGER;
    }
    else if (inputWaterPercent >= 70 ||
             outputWaterPercent >= 70)
    {
        currentAlertLevel = ALERT_HIGH;
    }
    else
    {
        currentAlertLevel = ALERT_NORMAL;
    }
}

void updateAlertOutputs()
{
    switch (currentAlertLevel)
    {
        case ALERT_NORMAL:
            digitalWrite(ALERT_LED_PIN, LOW);
            digitalWrite(BUZZER_PIN, BUZZER_OFF);
            break;

        case ALERT_HIGH:
            digitalWrite(ALERT_LED_PIN, HIGH);
            digitalWrite(BUZZER_PIN, BUZZER_OFF);
            break;

        case ALERT_DANGER:
        {
            bool blinkState =
                (millis() / DANGER_BLINK_INTERVAL) % 2 == 0;

            digitalWrite(
                ALERT_LED_PIN,
                blinkState ? HIGH : LOW
            );

            digitalWrite(
                BUZZER_PIN,
                blinkState ? BUZZER_ON : BUZZER_OFF
            );

            break;
        }
    }
}

void updateAlertSystem()
{
    determineAlertLevel();
    updateAlertOutputs();

    if (currentAlertLevel != previousAlertLevel)
    {
        previousAlertLevel = currentAlertLevel;

        Serial.print("[ALERT] ");
        Serial.println(getAlertLevelName());
    }
}

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