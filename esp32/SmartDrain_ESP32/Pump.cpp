#include <Arduino.h>

#include "Config.h"
#include "WaterSensor.h"
#include "Pump.h"

bool pumpRunning = false;
OperationMode operationMode = MODE_AUTO;

const int PUMP_START_LEVEL = 70;
const int PUMP_STOP_LEVEL = 30;
const int OUTPUT_DANGER_LEVEL = 80;

void setupPump()
{
    pinMode(RELAY_PIN, OUTPUT);

    // Khởi động an toàn: máy bơm luôn tắt
    digitalWrite(RELAY_PIN, RELAY_OFF);

    pumpRunning = false;
    operationMode = MODE_AUTO;
}

void setPump(bool state)
{
    // Không cho bật bơm nếu bể xả đã đầy
    if (state && outputWaterPercent >= OUTPUT_DANGER_LEVEL)
    {
        state = false;
        Serial.println("[SAFETY] Khong the bat bom: be xa da day");
    }

    // Không ghi relay lại nếu trạng thái không đổi
    if (pumpRunning == state)
    {
        return;
    }

    pumpRunning = state;
    digitalWrite(RELAY_PIN, state ? RELAY_ON : RELAY_OFF);

    Serial.print("[PUMP] ");
    Serial.println(pumpRunning ? "ON" : "OFF");
}

void setOperationMode(OperationMode newMode)
{
    if (operationMode == newMode)
    {
        return;
    }

    operationMode = newMode;

    // Khi đổi chế độ, tắt bơm trước để bảo đảm an toàn.
    // Nếu chuyển sang Auto, updatePump() sẽ tự quyết định bật lại.
    setPump(false);

    Serial.print("[MODE] ");
    Serial.println(getOperationModeName());
}

void toggleOperationMode()
{
    if (operationMode == MODE_AUTO)
    {
        setOperationMode(MODE_MANUAL);
    }
    else
    {
        setOperationMode(MODE_AUTO);
    }
}

void toggleManualPump()
{
    if (operationMode != MODE_MANUAL)
    {
        Serial.println("[BUTTON] Bo qua: he thong dang o AUTO");
        return;
    }

    if (!pumpRunning &&
        outputWaterPercent >= OUTPUT_DANGER_LEVEL)
    {
        Serial.println("[SAFETY] Khong the bat bom: be xa da day");
        return;
    }

    setPump(!pumpRunning);
}

void updatePump()
{
    // Chống tràn hoạt động trong cả Auto và Manual
    if (outputWaterPercent >= OUTPUT_DANGER_LEVEL)
    {
        setPump(false);
        return;
    }

    // Trong Manual, bơm chỉ được điều khiển bằng nút hoặc MQTT
    if (operationMode == MODE_MANUAL)
    {
        return;
    }

    // Logic Auto với vùng trễ 70% - 30%
    if (!pumpRunning &&
        inputWaterPercent >= PUMP_START_LEVEL)
    {
        setPump(true);
    }
    else if (pumpRunning &&
             inputWaterPercent <= PUMP_STOP_LEVEL)
    {
        setPump(false);
    }
}

const char* getOperationModeName()
{
    return operationMode == MODE_AUTO
        ? "AUTO"
        : "MANUAL";
}