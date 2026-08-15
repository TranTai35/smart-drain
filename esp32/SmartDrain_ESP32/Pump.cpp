#include <Arduino.h>

#include "Config.h"
#include "RuntimeConfig.h"
#include "WaterSensor.h"
#include "Pump.h"

// =========================
// CURRENT STATE
// =========================

bool pumpRunning = false;

OperationMode operationMode = MODE_AUTO;
PumpFault pumpFault = PUMP_FAULT_NONE;
PumpSource pumpSource = PUMP_SOURCE_BOOT;
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
    pumpSource = PUMP_SOURCE_BOOT;

    pumpStartTime = 0;
    drainCheckStartTime = 0;
    drainCheckStartPercent = 0;
}

// =========================
// BASIC PUMP CONTROL
// =========================

void setPump(
    bool state,
    PumpSource source)
{
    if (state &&
        pumpFault != PUMP_FAULT_NONE)
    {
        pumpSource = PUMP_SOURCE_SAFETY;

        Serial.print(
            "[SAFETY] Khong the bat bom: "
        );
        Serial.println(getPumpFaultName());

        return;
    }

    if (state &&
        outputWaterPercent >= outputLimit)
    {
        pumpSource = PUMP_SOURCE_SAFETY;

        Serial.println(
            "[SAFETY] Khong the bat bom: "
            "be xa da day"
        );

        return;
    }

    if (pumpRunning == state)
    {
        // Khi hệ thống đang cưỡng chế an toàn,
        // vẫn cần báo source là SAFETY
        if (source == PUMP_SOURCE_SAFETY)
        {
            pumpSource = PUMP_SOURCE_SAFETY;
        }

        return;
    }

    pumpRunning = state;
    pumpSource = source;

    digitalWrite(
        RELAY_PIN,
        pumpRunning
            ? RELAY_ON
            : RELAY_OFF
    );

    if (pumpRunning)
    {
        pumpStartTime = millis();
        drainCheckStartTime = millis();
        drainCheckStartPercent =
            inputWaterPercent;
    }
    else
    {
        pumpStartTime = 0;
        drainCheckStartTime = 0;
    }

    Serial.print("[PUMP] ");
    Serial.print(
        pumpRunning ? "ON" : "OFF"
    );

    Serial.print(" | Source: ");
    Serial.println(getPumpSourceName());
}

// =========================
// OPERATION MODE
// =========================

void setOperationMode(
    OperationMode newMode)
{
    if (operationMode == newMode)
    {
        return;
    }

    // Đổi chế độ được xem là
    // thao tác xác nhận lỗi
    clearPumpFault();

    // Luôn tắt bơm trước khi đổi chế độ
    setPump(
    false,
    PUMP_SOURCE_MANUAL
    );

    operationMode = newMode;

    Serial.print("[MODE] ");
    Serial.println(
        getOperationModeName()
    );
}

void toggleOperationMode()
{
    if (operationMode == MODE_AUTO)
    {
        setOperationMode(
            MODE_MANUAL
        );
    }
    else
    {
        setOperationMode(
            MODE_AUTO
        );
    }
}

// =========================
// MANUAL PUMP CONTROL
// =========================

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

    // Nếu có lỗi chốt, lần bấm đầu
    // chỉ dùng để xác nhận và xóa lỗi.
    // Bơm vẫn giữ trạng thái OFF.
    if (pumpFault != PUMP_FAULT_NONE)
    {
        clearPumpFault();
        setPump(
            false,
            PUMP_SOURCE_MANUAL
        );

        Serial.println(
            "[BUTTON] Da xoa loi, "
            "bom van OFF"
        );

        return;
    }

    setPump(
    !pumpRunning,
    PUMP_SOURCE_MANUAL
    );
}

// =========================
// PUMP TIMEOUT
// =========================

void triggerPumpTimeout()
{
    if (pumpFault ==
        PUMP_FAULT_TIMEOUT)
    {
        return;
    }

    pumpFault =
        PUMP_FAULT_TIMEOUT;

    // Tắt bơm nhưng giữ lỗi chốt
    setPump(
    false,
    PUMP_SOURCE_SAFETY
    );

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

    pumpFault =
        PUMP_FAULT_DRAIN_ABNORMAL;

    // Tắt bơm nhưng giữ lỗi chốt
    setPump(
    false,
    PUMP_SOURCE_SAFETY
    );

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

    unsigned long currentTime =
        millis();

    // Chưa đến thời điểm kiểm tra
    if (currentTime -
            drainCheckStartTime <
        drainCheckIntervalMs)
    {
        return;
    }

    int waterDrop =
        drainCheckStartPercent -
        inputWaterPercent;

    Serial.print(
        "[DRAIN CHECK] Start: "
    );
    Serial.print(
        drainCheckStartPercent
    );

    Serial.print("% | Current: ");
    Serial.print(
        inputWaterPercent
    );

    Serial.print("% | Drop: ");
    Serial.print(waterDrop);
    Serial.println("%");

    // Mực nước không giảm đủ
    // theo cấu hình hiện tại
    if (waterDrop <
        drainMinDropPercent)
    {
        triggerDrainAbnormal();
        return;
    }

    // Chu kỳ hiện tại đạt yêu cầu.
    // Bắt đầu chu kỳ kiểm tra tiếp theo.
    drainCheckStartTime =
        currentTime;

    drainCheckStartPercent =
        inputWaterPercent;

    Serial.println(
        "[DRAIN CHECK] "
        "Hoat dong binh thuong"
    );
}

// =========================
// FAULT CONTROL
// =========================

void clearPumpFault()
{
    if (pumpFault ==
        PUMP_FAULT_NONE)
    {
        return;
    }

    pumpFault =
        PUMP_FAULT_NONE;

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

    return millis() -
        pumpStartTime;
}

// =========================
// MAIN CONTROL
// =========================

void updatePump()
{
    // 1. Chống tràn luôn hoạt động
    // trong cả AUTO và MANUAL
    if (outputWaterPercent >=
        outputLimit)
    {
        if (pumpRunning)
        {
            Serial.println(
                "[SAFETY] Be xa da day, "
                "dung bom"
            );
        }

        setPump(
            false,
            PUMP_SOURCE_SAFETY
        );
        return;
    }

    // 2. Giới hạn thời gian
    // bơm chạy liên tục
    if (pumpRunning &&
        getPumpRuntimeMs() >=
            maxPumpRuntimeMs)
    {
        triggerPumpTimeout();
        return;
    }

    // 3. Kiểm tra hiệu quả thoát nước
    updateDrainEffectiveness();

    // updateDrainEffectiveness() có thể
    // vừa tạo lỗi DRAIN_ABNORMAL
    if (pumpFault !=
        PUMP_FAULT_NONE)
    {
        setPump(
            false,
            PUMP_SOURCE_SAFETY
        );
        return;
    }

    // 4. MANUAL chỉ được điều khiển
    // bằng nút bấm hoặc MQTT
    if (operationMode ==
        MODE_MANUAL)
    {
        return;
    }

    // 5. AUTO sử dụng vùng trễ động
    if (!pumpRunning &&
        inputWaterPercent >=
            pumpStartLevel)
    {
        setPump(
            true,
            PUMP_SOURCE_AUTO
        );
    }
    else if (
        pumpRunning &&
        inputWaterPercent <=
            pumpStopLevel)
    {
        setPump(
            false,
            PUMP_SOURCE_AUTO
        );
    }
}

// =========================
// STATE NAMES
// =========================

const char* getOperationModeName()
{
    return operationMode ==
        MODE_AUTO
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

const char* getPumpSourceName()
{
    switch (pumpSource)
    {
        case PUMP_SOURCE_AUTO:
            return "AUTO";

        case PUMP_SOURCE_MANUAL:
            return "MANUAL";

        case PUMP_SOURCE_SAFETY:
            return "SAFETY";

        default:
            return "BOOT";
    }
}