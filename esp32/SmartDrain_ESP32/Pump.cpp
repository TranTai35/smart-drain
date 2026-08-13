#include <Arduino.h>
#include "Config.h"
#include "WaterSensor.h"
#include "Pump.h"

bool pumpRunning = false;

const int PUMP_START_LEVEL = 70;
const int PUMP_STOP_LEVEL = 50;
const int OUTPUT_DANGER_LEVEL = 80;

void setupPump()
{
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, RELAY_OFF);
    pumpRunning = false;
}

void setPump(bool state)
{
    pumpRunning = state;
    digitalWrite(RELAY_PIN, state ? RELAY_ON : RELAY_OFF);
}

void updatePump()
{
    if (outputWaterPercent >= OUTPUT_DANGER_LEVEL)
    {
        setPump(false);
        return;
    }

    if (!pumpRunning && inputWaterPercent >= PUMP_START_LEVEL)
    {
        setPump(true);
    }

    if (pumpRunning && inputWaterPercent <= PUMP_STOP_LEVEL)
    {
        setPump(false);
    }
}