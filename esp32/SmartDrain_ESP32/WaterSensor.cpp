#include <Arduino.h>
#include "Config.h"
#include "WaterSensor.h"

// =========================
// CURRENT VALUES
// =========================

int inputWaterADC = 0;
int outputWaterADC = 0;

int inputWaterPercent = 0;
int outputWaterPercent = 0;

// =========================
// INPUT TANK CALIBRATION
// =========================

const int INPUT_ADC_0 = 0;
const int INPUT_ADC_25 = 1850;
const int INPUT_ADC_50 = 2000;
const int INPUT_ADC_75 = 2150;
const int INPUT_ADC_100 = 2300;

// =========================
// OUTPUT TANK CALIBRATION
// =========================

const int OUTPUT_ADC_0 = 0;
const int OUTPUT_ADC_25 = 1770;
const int OUTPUT_ADC_50 = 1920;
const int OUTPUT_ADC_75 = 2030;
const int OUTPUT_ADC_100 = 2150;

// =========================
// ADC FILTER
// =========================

int readAverageADC(int pin)
{
    const int SAMPLE_COUNT = 10;
    long sum = 0;

    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        sum += analogRead(pin);
        delay(2);
    }

    return sum / SAMPLE_COUNT;
}

// =========================
// ADC -> PERCENT
// =========================

int adcToPercent(int adc, int adc0, int adc25, int adc50, int adc75, int adc100)
{
    if (adc <= adc0) return 0;

    if (adc <= adc25)
        return map(adc, adc0, adc25, 0, 25);

    if (adc <= adc50)
        return map(adc, adc25, adc50, 25, 50);

    if (adc <= adc75)
        return map(adc, adc50, adc75, 50, 75);

    if (adc <= adc100)
        return map(adc, adc75, adc100, 75, 100);

    return 100;
}

// =========================
// WATER LEVEL STATUS
// =========================

const char* getWaterLevelName(int percent)
{
    if (percent < 30) return "LOW";
    if (percent < 70) return "MEDIUM";
    if (percent < 90) return "HIGH";

    return "DANGER";
}

// =========================
// SETUP
// =========================

void setupWaterSensors()
{
    pinMode(INPUT_WATER_PIN, INPUT);
    pinMode(OUTPUT_WATER_PIN, INPUT);
}

// =========================
// UPDATE
// =========================

void updateWaterSensors()
{
    inputWaterADC = readAverageADC(INPUT_WATER_PIN);
    outputWaterADC = readAverageADC(OUTPUT_WATER_PIN);

    inputWaterPercent = adcToPercent(inputWaterADC, INPUT_ADC_0, INPUT_ADC_25, INPUT_ADC_50, INPUT_ADC_75, INPUT_ADC_100);
    outputWaterPercent = adcToPercent(outputWaterADC, OUTPUT_ADC_0, OUTPUT_ADC_25, OUTPUT_ADC_50, OUTPUT_ADC_75, OUTPUT_ADC_100);
}