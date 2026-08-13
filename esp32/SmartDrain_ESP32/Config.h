#ifndef CONFIG_H
#define CONFIG_H

// Water Sensor
#define INPUT_WATER_PIN 34
#define OUTPUT_WATER_PIN 35

// Relay + Pump
#define RELAY_PIN 26
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// LCD I2C
#define LCD_SDA_PIN 21
#define LCD_SCL_PIN 22
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

#endif