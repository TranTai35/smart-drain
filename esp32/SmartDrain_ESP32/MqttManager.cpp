#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <string.h>
#include <stdlib.h>

#include "Config.h"
#include "RuntimeConfig.h"
#include "NetworkManager.h"
#include "MqttManager.h"
#include "WaterSensor.h"
#include "Pump.h"
#include "AlertSystem.h"

// =========================
// MQTT CLIENT
// =========================

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// =========================
// INTERVALS
// =========================

const unsigned long
    MQTT_RETRY_INTERVAL = 5000UL;

const unsigned long
    TANK_PUBLISH_INTERVAL = 2000UL;

const unsigned long
    STATE_HEARTBEAT_INTERVAL = 10000UL;

// =========================
// TIMERS
// =========================

unsigned long lastMqttAttempt = 0;
unsigned long lastTankPublish = 0;
unsigned long lastStateHeartbeat = 0;

// =========================
// LAST PUBLISHED STATE
// =========================

bool stateSnapshotInitialized = false;

bool lastPublishedPumpRunning = false;
bool lastPublishedBuzzerMuted = false;

OperationMode lastPublishedMode =
    MODE_AUTO;

PumpFault lastPublishedFault =
    PUMP_FAULT_NONE;

AlertLevel lastPublishedAlertLevel =
    ALERT_NORMAL;

// =========================
// PUMP COMMAND
// =========================

void handlePumpCommand(
    const char* message)
{
    Serial.print(
        "[MQTT COMMAND] Pump: "
    );
    Serial.println(message);

    // OFF luôn được chấp nhận
    if (strcmp(message, "OFF") == 0)
    {
        clearPumpFault();
        setPump(false);

        updateAlertSystem();

        publishPumpState();
        publishAlert();

        return;
    }

    if (strcmp(message, "ON") == 0)
    {
        // Website không được trực tiếp bật
        // bơm khi hệ thống đang ở AUTO
        if (operationMode == MODE_AUTO)
        {
            Serial.println(
                "[MQTT COMMAND] Tu choi ON: "
                "dang AUTO"
            );

            publishPumpState();
            publishAlert();

            return;
        }

        // setPump() tự kiểm tra:
        // - lỗi chốt
        // - bể xả đầy
        setPump(true);

        updateAlertSystem();

        // Luôn trả lại trạng thái thật
        publishPumpState();
        publishAlert();

        return;
    }

    Serial.println(
        "[MQTT COMMAND] "
        "Lenh pump khong hop le"
    );

    publishPumpState();
    publishAlert();
}

// =========================
// MODE COMMAND
// =========================

void handleModeCommand(
    const char* message)
{
    Serial.print(
        "[MQTT COMMAND] Mode: "
    );
    Serial.println(message);

    if (strcmp(message, "AUTO") == 0)
    {
        clearPumpFault();
        setOperationMode(MODE_AUTO);

        updateAlertSystem();

        publishMode();
        publishPumpState();
        publishAlert();

        return;
    }

    if (strcmp(message, "MANUAL") == 0)
    {
        clearPumpFault();
        setOperationMode(MODE_MANUAL);

        updateAlertSystem();

        publishMode();
        publishPumpState();
        publishAlert();

        return;
    }

    Serial.println(
        "[MQTT COMMAND] "
        "Lenh mode khong hop le"
    );

    publishMode();
    publishPumpState();
    publishAlert();
}

// =========================
// BUZZER COMMAND
// =========================

void handleBuzzerCommand(
    const char* message)
{
    Serial.print(
        "[MQTT COMMAND] Buzzer: "
    );
    Serial.println(message);

    if (strcmp(message, "MUTE") == 0)
    {
        setBuzzerMuted(true);

        publishAlert();

        lastPublishedBuzzerMuted =
            isBuzzerMuted();

        return;
    }

    if (strcmp(message, "UNMUTE") == 0)
    {
        setBuzzerMuted(false);

        publishAlert();

        lastPublishedBuzzerMuted =
            isBuzzerMuted();

        return;
    }

    Serial.println(
        "[MQTT COMMAND] "
        "Lenh buzzer khong hop le"
    );

    publishAlert();
}

// =========================
// CONFIG COMMAND
// =========================

void handleConfigCommand(
    const char* message)
{
    Serial.print(
        "[MQTT COMMAND] Config: "
    );
    Serial.println(message);

    char key[32];
    long value = 0;
    char extraCharacter = '\0';

    // Lệnh phải có dạng:
    // KEY=VALUE
    //
    // Ví dụ:
    // pump_start=75
    int parsedItems = sscanf(
        message,
        "%31[^=]=%ld%c",
        key,
        &value,
        &extraCharacter
    );

    // Nếu khác 2 nghĩa là:
    // - thiếu dấu =
    // - thiếu key
    // - thiếu value
    // - có ký tự thừa phía sau value
    if (parsedItems != 2)
    {
        Serial.println(
            "[MQTT COMMAND] "
            "Sai dinh dang config. "
            "Can dung KEY=VALUE"
        );

        // Dù thất bại vẫn phải trả lại
        // cấu hình thật đang áp dụng
        publishConfig();

        return;
    }

    bool success =
        applyRuntimeConfig(
            key,
            value
        );

    if (success)
    {
        Serial.println(
            "[MQTT COMMAND] "
            "Cap nhat config thanh cong"
        );

        // Áp dụng điều kiện an toàn mới ngay lập tức
        updatePump();
        updateAlertSystem();
    }
    else
    {
        Serial.println(
            "[MQTT COMMAND] "
            "Tu choi gia tri config"
        );
    }

    // Theo giao thức:
    // Sau khi nhận lệnh hợp lệ hoặc không hợp lệ,
    // ESP32 đều phải publish cấu hình đang áp dụng.
    publishConfig();

    // Trả lại các trạng thái có thể bị ảnh hưởng
    publishPumpState();
    publishAlert();
}

// =========================
// RECEIVE MQTT MESSAGE
// =========================

void onMqttMessage(
    char* topic,
    byte* payload,
    unsigned int length)
{
    char message[128];

    if (length >= sizeof(message))
    {
        length =
            sizeof(message) - 1;
    }

    memcpy(
        message,
        payload,
        length
    );

    message[length] = '\0';

    Serial.print("[MQTT] Topic: ");
    Serial.println(topic);

    Serial.print("[MQTT] Message: ");
    Serial.println(message);

    if (strcmp(
            topic,
            TOPIC_COMMAND_PUMP
        ) == 0)
    {
        handlePumpCommand(message);
    }
    else if (strcmp(
                 topic,
                 TOPIC_COMMAND_MODE
             ) == 0)
    {
        handleModeCommand(message);
    }
    else if (strcmp(
                 topic,
                 TOPIC_COMMAND_BUZZER
             ) == 0)
    {
        handleBuzzerCommand(message);
    }
    else if (strcmp(
                 topic,
                 TOPIC_COMMAND_CONFIG
             ) == 0)
    {
        handleConfigCommand(message);
    }
    else
    {
        Serial.println(
            "[MQTT] Topic khong duoc ho tro"
        );
    }
}

// =========================
// PUBLISH TANK
// =========================

void publishTank(
    const char* topic,
    int percent,
    int adc)
{
    if (!mqttClient.connected())
    {
        return;
    }

    char payload[160];

    snprintf(
        payload,
        sizeof(payload),
        "{\"percent\":%d,"
        "\"adc\":%d,"
        "\"level\":\"%s\","
        "\"uptime\":%lu}",
        percent,
        adc,
        getWaterLevelName(percent),
        millis() / 1000UL
    );

    bool success =
        mqttClient.publish(
            topic,
            payload,
            true
        );

    Serial.print("[MQTT PUBLISH] ");
    Serial.print(topic);
    Serial.print(" -> ");
    Serial.print(payload);

    Serial.println(
        success
            ? " [OK]"
            : " [FAILED]"
    );
}

void publishTankData()
{
    publishTank(
        TOPIC_TANK_INPUT,
        inputWaterPercent,
        inputWaterADC
    );

    publishTank(
        TOPIC_TANK_OUTPUT,
        outputWaterPercent,
        outputWaterADC
    );

    lastTankPublish = millis();
}

// =========================
// PUMP SOURCE
// =========================

const char* getMqttPumpSource()
{
    return getPumpSourceName();
}

// =========================
// PUBLISH PUMP
// =========================

void publishPumpState()
{
    if (!mqttClient.connected())
    {
        return;
    }

    char payload[160];

    snprintf(
        payload,
        sizeof(payload),
        "{\"state\":\"%s\","
        "\"source\":\"%s\","
        "\"runtime\":%lu,"
        "\"uptime\":%lu}",
        pumpRunning
            ? "ON"
            : "OFF",
        getMqttPumpSource(),
        getPumpRuntimeMs() / 1000UL,
        millis() / 1000UL
    );

    bool success =
        mqttClient.publish(
            TOPIC_PUMP_STATE,
            payload,
            true
        );

    Serial.print("[MQTT PUBLISH] ");
    Serial.print(TOPIC_PUMP_STATE);
    Serial.print(" -> ");
    Serial.print(payload);

    Serial.println(
        success
            ? " [OK]"
            : " [FAILED]"
    );
}

// =========================
// PUBLISH MODE
// =========================

void publishMode()
{
    if (!mqttClient.connected())
    {
        return;
    }

    char payload[96];

    snprintf(
        payload,
        sizeof(payload),
        "{\"mode\":\"%s\","
        "\"uptime\":%lu}",
        getOperationModeName(),
        millis() / 1000UL
    );

    bool success =
        mqttClient.publish(
            TOPIC_MODE,
            payload,
            true
        );

    Serial.print("[MQTT PUBLISH] ");
    Serial.print(TOPIC_MODE);
    Serial.print(" -> ");
    Serial.print(payload);

    Serial.println(
        success
            ? " [OK]"
            : " [FAILED]"
    );
}

// =========================
// ALERT INFORMATION
// =========================

const char* getMqttAlertCode()
{
    if (outputWaterPercent >=
        outputLimit)
    {
        return "OUTPUT_TANK_FULL";
    }

    if (inputWaterPercent >= 90)
    {
        return "INPUT_TANK_DANGER";
    }

    if (pumpFault ==
        PUMP_FAULT_TIMEOUT)
    {
        return "PUMP_TIMEOUT";
    }

    if (pumpFault ==
        PUMP_FAULT_DRAIN_ABNORMAL)
    {
        return "DRAIN_ABNORMAL";
    }

    return "NONE";
}

const char* getMqttAlertSeverity()
{
    if (outputWaterPercent >=
            outputLimit ||
        inputWaterPercent >= 90)
    {
        return "DANGER";
    }

    if (pumpFault !=
        PUMP_FAULT_NONE)
    {
        return "WARNING";
    }

    return "INFO";
}

const char* getMqttAlertMessage()
{
    if (outputWaterPercent >=
        outputLimit)
    {
        return "Be xa da day, da dung bom";
    }

    if (inputWaterPercent >= 90)
    {
        return "Be thu o muc nguy hiem";
    }

    if (pumpFault ==
        PUMP_FAULT_TIMEOUT)
    {
        return
            "Bom chay qua thoi gian cho phep";
    }

    if (pumpFault ==
        PUMP_FAULT_DRAIN_ABNORMAL)
    {
        return
            "Thoat nuoc bat thuong, kiem tra ong";
    }

    return "He thong binh thuong";
}

// =========================
// PUBLISH ALERT
// =========================

void publishAlert()
{
    if (!mqttClient.connected())
    {
        return;
    }

    bool alertActive =
        strcmp(
            getMqttAlertCode(),
            "NONE"
        ) != 0;

    char payload[280];

    snprintf(
        payload,
        sizeof(payload),
        "{\"code\":\"%s\","
        "\"severity\":\"%s\","
        "\"message\":\"%s\","
        "\"active\":%s,"
        "\"muted\":%s,"
        "\"uptime\":%lu}",
        getMqttAlertCode(),
        getMqttAlertSeverity(),
        getMqttAlertMessage(),
        alertActive
            ? "true"
            : "false",
        isBuzzerMuted()
            ? "true"
            : "false",
        millis() / 1000UL
    );

    bool success =
        mqttClient.publish(
            TOPIC_ALERT,
            payload,
            true
        );

    Serial.print("[MQTT PUBLISH] ");
    Serial.print(TOPIC_ALERT);
    Serial.print(" -> ");
    Serial.print(payload);

    Serial.println(
        success
            ? " [OK]"
            : " [FAILED]"
    );
}

// =========================
// PUBLISH CONFIG
// =========================

void publishConfig()
{
    if (!mqttClient.connected())
    {
        return;
    }

    char payload[240];

    snprintf(
        payload,
        sizeof(payload),
        "{\"pump_start\":%d,"
        "\"pump_stop\":%d,"
        "\"output_limit\":%d,"
        "\"max_runtime\":%lu,"
        "\"drain_check\":%lu,"
        "\"drain_min_drop\":%d}",
        pumpStartLevel,
        pumpStopLevel,
        outputLimit,
        maxPumpRuntimeMs / 1000UL,
        drainCheckIntervalMs / 1000UL,
        drainMinDropPercent
    );

    bool success =
        mqttClient.publish(
            TOPIC_CONFIG,
            payload,
            true
        );

    Serial.print("[MQTT PUBLISH] ");
    Serial.print(TOPIC_CONFIG);
    Serial.print(" -> ");
    Serial.print(payload);

    Serial.println(
        success
            ? " [OK]"
            : " [FAILED]"
    );
}

// =========================
// STATE + HEARTBEAT
// =========================

void updateStatePublishing()
{
    if (!mqttClient.connected())
    {
        return;
    }

    unsigned long currentTime =
        millis();

    bool stateChanged =
        !stateSnapshotInitialized ||
        pumpRunning !=
            lastPublishedPumpRunning ||
        operationMode !=
            lastPublishedMode ||
        pumpFault !=
            lastPublishedFault ||
        currentAlertLevel !=
            lastPublishedAlertLevel ||
        isBuzzerMuted() !=
            lastPublishedBuzzerMuted;

    bool heartbeatDue =
        currentTime -
            lastStateHeartbeat >=
        STATE_HEARTBEAT_INTERVAL;

    if (!stateChanged &&
        !heartbeatDue)
    {
        return;
    }

    publishPumpState();
    publishMode();
    publishAlert();
    publishConfig();

    lastPublishedPumpRunning =
        pumpRunning;

    lastPublishedMode =
        operationMode;

    lastPublishedFault =
        pumpFault;

    lastPublishedAlertLevel =
        currentAlertLevel;

    lastPublishedBuzzerMuted =
        isBuzzerMuted();

    stateSnapshotInitialized = true;
    lastStateHeartbeat = currentTime;
}

// =========================
// CONNECT MQTT
// =========================

bool connectMqtt()
{
    Serial.print(
        "[MQTT] Dang ket noi toi: "
    );
    Serial.println(MQTT_HOST);

    // Kết nối cùng Last Will:
    // ESP32 mất kết nối đột ngột
    // thì broker giữ trạng thái OFFLINE
    bool connected =
        mqttClient.connect(
            MQTT_CLIENT_ID,
            NULL,
            NULL,
            TOPIC_STATUS,
            1,
            true,
            "OFFLINE"
        );

    if (!connected)
    {
        Serial.print(
            "[MQTT] Ket noi that bai, "
            "state: "
        );

        Serial.println(
            mqttClient.state()
        );

        return false;
    }

    Serial.println(
        "[MQTT] Ket noi thanh cong"
    );

    mqttClient.publish(
        TOPIC_STATUS,
        "ONLINE",
        true
    );

    mqttClient.subscribe(
        TOPIC_COMMAND_PUMP,
        1
    );

    mqttClient.subscribe(
        TOPIC_COMMAND_MODE,
        1
    );

    mqttClient.subscribe(
        TOPIC_COMMAND_BUZZER,
        1
    );

    mqttClient.subscribe(
        TOPIC_COMMAND_CONFIG,
        1
    );

    Serial.println(
        "[MQTT] Da subscribe "
        "4 topic command"
    );

    // Gửi dữ liệu ngay khi kết nối
    publishTankData();

    // Buộc gửi toàn bộ trạng thái
    // sau mỗi lần kết nối lại
    stateSnapshotInitialized = false;
    updateStatePublishing();

    return true;
}

// =========================
// SETUP MQTT
// =========================

void setupMqttManager()
{
    mqttClient.setServer(
        MQTT_HOST,
        MQTT_PORT
    );

    mqttClient.setCallback(
        onMqttMessage
    );

    mqttClient.setKeepAlive(
        MQTT_KEEP_ALIVE
    );

    mqttClient.setBufferSize(
        MQTT_BUFFER_SIZE
    );
}

// =========================
// UPDATE MQTT
// =========================

void updateMqttManager()
{
    // Mất Wi-Fi không được làm ảnh hưởng
    // đến điều khiển cục bộ
    if (!isWiFiConnected())
    {
        return;
    }

    if (!mqttClient.connected())
    {
        unsigned long currentTime =
            millis();

        if (currentTime -
                lastMqttAttempt >=
            MQTT_RETRY_INTERVAL)
        {
            lastMqttAttempt =
                currentTime;

            connectMqtt();
        }

        return;
    }

    // Phải gọi liên tục để nhận lệnh
    mqttClient.loop();

    unsigned long currentTime =
        millis();

    // Publish dữ liệu hai bể mỗi 2 giây
    if (currentTime -
            lastTankPublish >=
        TANK_PUBLISH_INTERVAL)
    {
        publishTankData();
    }

    // Publish khi trạng thái thay đổi
    // hoặc heartbeat mỗi 10 giây
    updateStatePublishing();
}

// =========================
// CONNECTION STATE
// =========================

bool isMqttConnected()
{
    return mqttClient.connected();
}