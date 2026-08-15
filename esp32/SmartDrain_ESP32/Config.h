#ifndef CONFIG_H
#define CONFIG_H

// =========================
// WATER SENSOR
// =========================

#define INPUT_WATER_PIN 34
#define OUTPUT_WATER_PIN 35

// =========================
// RELAY + PUMP
// =========================

#define RELAY_PIN 26
#define RELAY_ON HIGH
#define RELAY_OFF LOW

// =========================
// LCD I2C
// =========================

#define LCD_SDA_PIN 21
#define LCD_SCL_PIN 22
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

// =========================
// BUTTONS
// =========================

#define MODE_BUTTON_PIN 32
#define PUMP_BUTTON_PIN 33
#define BUTTON_DEBOUNCE_MS 50

// =========================
// ALERT LED + BUZZER
// =========================

#define ALERT_LED_PIN 25
#define BUZZER_PIN 27

#define BUZZER_ON HIGH
#define BUZZER_OFF LOW

// =========================
// PUMP SAFETY
// =========================

#define PUMP_START_LEVEL 70
#define PUMP_STOP_LEVEL 30
#define OUTPUT_DANGER_LEVEL 80

#define MAX_PUMP_RUNTIME_MS 120000UL
#define DRAIN_CHECK_INTERVAL_MS 30000UL
#define DRAIN_MIN_DROP_PERCENT 5

// =========================
// WI-FI
// =========================

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// =========================
// MQTT BROKER
// =========================

#define MQTT_HOST "broker.hivemq.com"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "smartdrain-esp32"
#define MQTT_KEEP_ALIVE 30
#define MQTT_BUFFER_SIZE 512

// Không tự thay BASE nếu chưa báo bên website
#define MQTT_BASE "smartdrain"

// =========================
// ESP32 -> WEBSITE
// =========================

#define TOPIC_TANK_INPUT MQTT_BASE "/tank/input"
#define TOPIC_TANK_OUTPUT MQTT_BASE "/tank/output"
#define TOPIC_PUMP_STATE MQTT_BASE "/pump/state"
#define TOPIC_MODE MQTT_BASE "/mode"
#define TOPIC_ALERT MQTT_BASE "/alert"
#define TOPIC_STATUS MQTT_BASE "/status"
#define TOPIC_CONFIG MQTT_BASE "/config"

// =========================
// WEBSITE -> ESP32
// =========================

#define TOPIC_COMMAND_PUMP MQTT_BASE "/command/pump"
#define TOPIC_COMMAND_MODE MQTT_BASE "/command/mode"
#define TOPIC_COMMAND_BUZZER MQTT_BASE "/command/buzzer"
#define TOPIC_COMMAND_CONFIG MQTT_BASE "/command/config"

#endif