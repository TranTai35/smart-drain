#ifndef CONFIG_H
#define CONFIG_H

// Water Sensor
#define INPUT_WATER_PIN 34
#define OUTPUT_WATER_PIN 35

// Relay + Pump
#define RELAY_PIN 26
#define RELAY_ON HIGH
#define RELAY_OFF LOW

// LCD I2C
#define LCD_SDA_PIN 21
#define LCD_SCL_PIN 22
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

// BUTTONS

#define MODE_BUTTON_PIN 32
#define PUMP_BUTTON_PIN 33
#define BUTTON_DEBOUNCE_MS 50

// ALERT LED + BUZZER

#define ALERT_LED_PIN 25
#define BUZZER_PIN 27

#define BUZZER_ON HIGH
#define BUZZER_OFF LOW

#endif