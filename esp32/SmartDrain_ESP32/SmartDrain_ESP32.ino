#include "WaterSensor.h"
#include "Display.h"
#include "Pump.h"
#include "Buttons.h"
#include "Config.h"

unsigned long lastSystemUpdate = 0;
const unsigned long SYSTEM_UPDATE_INTERVAL = 500;

void setup()
{
    Serial.begin(9600);

    setupWaterSensors();
    setupPump();
    setupButtons();
    setupDisplay();
}

void loop()
{
    // Phải đọc nút liên tục để không bỏ lỡ thao tác
    updateButtons();

    unsigned long currentTime = millis();

    if (currentTime - lastSystemUpdate >= SYSTEM_UPDATE_INTERVAL)
    {
        lastSystemUpdate = currentTime;

        updateWaterSensors();
        updatePump();
        updateDisplay();

        Serial.println("===== SMART DRAIN =====");

        Serial.print("Mode: ");
        Serial.println(getOperationModeName());

        Serial.print("Input ADC: ");
        Serial.print(inputWaterADC);
        Serial.print(" | Level: ");
        Serial.print(inputWaterPercent);
        Serial.print("% | ");
        Serial.println(getWaterLevelName(inputWaterPercent));

        Serial.print("Output ADC: ");
        Serial.print(outputWaterADC);
        Serial.print(" | Level: ");
        Serial.print(outputWaterPercent);
        Serial.print("% | ");
        Serial.println(getWaterLevelName(outputWaterPercent));

        Serial.print("Pump: ");
        Serial.println(pumpRunning ? "ON" : "OFF");

        Serial.println();
    }
}