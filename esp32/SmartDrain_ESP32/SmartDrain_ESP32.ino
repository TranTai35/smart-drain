#include "WaterSensor.h"
#include "Display.h"
#include "Pump.h"

void setup()
{
    Serial.begin(9600);

    setupWaterSensors();
    setupPump();
    setupDisplay();
}

void loop()
{
    updateWaterSensors();
    updatePump();
    updateDisplay();

    Serial.println("===== SMART DRAIN =====");

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

    delay(500);
}