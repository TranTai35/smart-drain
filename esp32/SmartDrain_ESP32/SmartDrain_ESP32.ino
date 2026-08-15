#include <WiFi.h>
#include "Config.h"
#include "WaterSensor.h"
#include "Display.h"
#include "Pump.h"
#include "Buttons.h"
#include "AlertSystem.h"
#include "NetworkManager.h"

unsigned long lastSystemUpdate = 0;
const unsigned long SYSTEM_UPDATE_INTERVAL = 500;

void setup()
{
    Serial.begin(9600);

    setupNetwork();
    setupWaterSensors();
    setupPump();
    setupButtons();
    setupDisplay();
    setupAlertSystem();
}

void loop()
{
    updateNetwork();
    // Nút phải được đọc liên tục
    updateButtons();

    unsigned long currentTime = millis();
    bool systemUpdated = false;

    if (currentTime - lastSystemUpdate >=
        SYSTEM_UPDATE_INTERVAL)
    {
        lastSystemUpdate = currentTime;

        // Thứ tự: đọc nước -> xử lý bơm -> cập nhật LCD
        updateWaterSensors();
        updatePump();
        updateDisplay();

        systemUpdated = true;
    }

    // Gọi liên tục để LED có thể nhấp nháy mượt
    // Hàm này chạy sau updatePump nên nhận lỗi mới ngay
    updateAlertSystem();

    if (systemUpdated)
    {
        Serial.println("===== SMART DRAIN =====");

        Serial.print("Mode: ");
        Serial.println(getOperationModeName());

        Serial.print("Input ADC: ");
        Serial.print(inputWaterADC);
        Serial.print(" | Level: ");
        Serial.print(inputWaterPercent);
        Serial.print("% | ");
        Serial.println(
            getWaterLevelName(inputWaterPercent)
        );

        Serial.print("Output ADC: ");
        Serial.print(outputWaterADC);
        Serial.print(" | Level: ");
        Serial.print(outputWaterPercent);
        Serial.print("% | ");
        Serial.println(
            getWaterLevelName(outputWaterPercent)
        );

        Serial.print("Pump: ");
        Serial.println(
            pumpRunning ? "ON" : "OFF"
        );

        Serial.print("Alert: ");
        Serial.println(getAlertLevelName());

        Serial.print("Pump fault: ");
        Serial.println(getPumpFaultName());

        Serial.print("Pump runtime: ");
        Serial.print(getPumpRuntimeMs() / 1000);
        Serial.println(" s");

        Serial.print("WiFi: ");
        Serial.println(
            isWiFiConnected()
                ? "CONNECTED"
                : "DISCONNECTED"
        );

        if (isWiFiConnected())
        {
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());
        }
        Serial.println();
    }
}