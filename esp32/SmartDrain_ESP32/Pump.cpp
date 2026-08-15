#include <Arduino.h>

#include "Config.h"
#include "WaterSensor.h"
#include "Pump.h"

// =========================
// CURRENT STATE
// =========================

bool pumpRunning = false;

OperationMode operationMode = MODE_AUTO;
PumpFault pumpFault = PUMP_FAULT_NONE;

// =========================
// TIMERS
// =========================

unsigned long pumpStartTime = 0;

unsigned long drainCheckStartTime = 0;
int drainCheckStartPercent = 0;

// =========================
// SETUP
// =========================

void setupPump()
{
    pinMode(RELAY_PIN, OUTPUT);

    // Khởi động an toàn: bơm luôn tắt
    digitalWrite(RELAY_PIN, RELAY_OFF);

    pumpRunning = false;
    operationMode = MODE_AUTO;
    pumpFault = PUMP_FAULT_NONE;

    pumpStartTime = 0;
    drainCheckStartTime = 0;
    drainCheckStartPercent = 0;
}

// =========================
// BASIC PUMP CONTROL
// =========================

void setPump(bool state)
{
    // Không cho bật bơm khi đang có lỗi chốt
    if (state &&
        pumpFault != PUMP_FAULT_NONE)
    {
        Serial.print(
            "[SAFETY] Khong the bat bom: "
        );
        Serial.println(getPumpFaultName());

        return;
    }

    // Không cho bật khi bể xả đã đầy
    if (state &&
        outputWaterPercent >= OUTPUT_DANGER_LEVEL)
    {
        Serial.println(
            "[SAFETY] Khong the bat bom: "
            "be xa da day"
        );

        return;
    }

    // Không ghi lại relay nếu trạng thái không đổi
    if (pumpRunning == state)
    {
        return;
    }

    pumpRunning = state;

    digitalWrite(
        RELAY_PIN,
        pumpRunning ? RELAY_ON : RELAY_OFF
    );

    if (pumpRunning)
    {
        pumpStartTime = millis();

        // Bắt đầu chu kỳ kiểm tra hiệu quả thoát nước
        drainCheckStartTime = millis();
        drainCheckStartPercent = inputWaterPercent;
    }
    else
    {
        pumpStartTime = 0;
        drainCheckStartTime = 0;
    }

    Serial.print("[PUMP] ");
    Serial.println(
        pumpRunning ? "ON" : "OFF"
    );
}

// =========================
// OPERATION MODE
// =========================

void setOperationMode(OperationMode newMode)
{
    if (operationMode == newMode)
    {
        return;
    }

    // Đổi chế độ được xem là xác nhận lỗi
    clearPumpFault();

    operationMode = newMode;

    // Tắt bơm trước khi áp dụng chế độ mới
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
        Serial.println(
            "[BUTTON] Bo qua: "
            "he thong dang o AUTO"
        );

        return;
    }

    // Nếu có lỗi chốt, lần bấm đầu chỉ xoá lỗi.
    // Bơm vẫn giữ OFF.
    if (pumpFault != PUMP_FAULT_NONE)
    {
        clearPumpFault();
        setPump(false);

        Serial.println(
            "[BUTTON] Da xoa loi, bom van OFF"
        );

        return;
    }

    if (pumpRunning)
    {
        setPump(false);
    }
    else
    {
        setPump(true);
    }
}

// =========================
// PUMP TIMEOUT
// =========================

void triggerPumpTimeout()
{
    if (pumpFault == PUMP_FAULT_TIMEOUT)
    {
        return;
    }

    pumpFault = PUMP_FAULT_TIMEOUT;

    // Tắt bơm nhưng giữ lỗi lại
    setPump(false);

    Serial.println(
        "[ALERT] PUMP_TIMEOUT - "
        "Bom chay qua thoi gian"
    );
}

// =========================
// ABNORMAL DRAIN
// =========================

void triggerDrainAbnormal()
{
    if (pumpFault ==
        PUMP_FAULT_DRAIN_ABNORMAL)
    {
        return;
    }

    pumpFault = PUMP_FAULT_DRAIN_ABNORMAL;

    // Tắt bơm nhưng giữ lỗi lại
    setPump(false);

    Serial.println(
        "[ALERT] DRAIN_ABNORMAL - "
        "Muc nuoc khong giam du"
    );
}

void updateDrainEffectiveness()
{
    if (!pumpRunning)
    {
        return;
    }

    unsigned long currentTime = millis();

    // Chưa đến thời điểm kiểm tra
    if (currentTime - drainCheckStartTime <
        DRAIN_CHECK_INTERVAL_MS)
    {
        return;
    }

    int waterDrop =
        drainCheckStartPercent -
        inputWaterPercent;

    Serial.print("[DRAIN CHECK] Start: ");
    Serial.print(drainCheckStartPercent);

    Serial.print("% | Current: ");
    Serial.print(inputWaterPercent);

    Serial.print("% | Drop: ");
    Serial.print(waterDrop);

    Serial.println("%");

    // Nước giảm chưa đủ mức tối thiểu
    if (waterDrop < DRAIN_MIN_DROP_PERCENT)
    {
        triggerDrainAbnormal();
        return;
    }

    // Chu kỳ hiện tại đạt yêu cầu.
    // Bắt đầu chu kỳ kiểm tra tiếp theo.
    drainCheckStartTime = currentTime;
    drainCheckStartPercent =
        inputWaterPercent;

    Serial.println(
        "[DRAIN CHECK] Hoat dong binh thuong"
    );
}

// =========================
// FAULT CONTROL
// =========================

void clearPumpFault()
{
    if (pumpFault == PUMP_FAULT_NONE)
    {
        return;
    }

    pumpFault = PUMP_FAULT_NONE;

    Serial.println(
        "[ALERT] Da xoa loi bom"
    );
}

// =========================
// RUNTIME
// =========================

unsigned long getPumpRuntimeMs()
{
    if (!pumpRunning)
    {
        return 0;
    }

    return millis() - pumpStartTime;
}

// =========================
// MAIN CONTROL
// =========================

void updatePump()
{
    // 1. Chống tràn hoạt động trong Auto và Manual
    if (outputWaterPercent >=
        OUTPUT_DANGER_LEVEL)
    {
        if (pumpRunning)
        {
            Serial.println(
                "[SAFETY] Be xa da day, dung bom"
            );
        }

        setPump(false);
        return;
    }

    // 2. Giới hạn thời gian chạy liên tục
    if (pumpRunning &&
        getPumpRuntimeMs() >=
        MAX_PUMP_RUNTIME_MS)
    {
        triggerPumpTimeout();
        return;
    }

    // 3. Kiểm tra hiệu quả thoát nước
    updateDrainEffectiveness();

    // Hàm trên có thể vừa tạo DRAIN_ABNORMAL
    if (pumpFault != PUMP_FAULT_NONE)
    {
        setPump(false);
        return;
    }

    // 4. Manual chỉ điều khiển bằng nút hoặc MQTT
    if (operationMode == MODE_MANUAL)
    {
        return;
    }

    // 5. Auto với vùng trễ 70% - 30%
    if (!pumpRunning &&
        inputWaterPercent >= PUMP_START_LEVEL)
    {
        setPump(true);
    }
    else if (pumpRunning &&
             inputWaterPercent <=
             PUMP_STOP_LEVEL)
    {
        setPump(false);
    }
}

// =========================
// STATE NAMES
// =========================

const char* getOperationModeName()
{
    return operationMode == MODE_AUTO
        ? "AUTO"
        : "MANUAL";
}

const char* getPumpFaultName()
{
    switch (pumpFault)
    {
        case PUMP_FAULT_TIMEOUT:
            return "PUMP_TIMEOUT";

        case PUMP_FAULT_DRAIN_ABNORMAL:
            return "DRAIN_ABNORMAL";

        default:
            return "NONE";
    }
}