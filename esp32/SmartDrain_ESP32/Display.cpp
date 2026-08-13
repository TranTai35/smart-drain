#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "Config.h"
#include "WaterSensor.h"
#include "Display.h"

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

void setupDisplay()
{
    Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);

    lcd.init();
    lcd.backlight();
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("SMART DRAIN");

    lcd.setCursor(0, 1);
    lcd.print("Starting...");

    delay(1000);
    lcd.clear();
}

void updateDisplay()
{
    lcd.setCursor(0, 0);
    lcd.print("IN:");
    lcd.print(inputWaterPercent);
    lcd.print("%   ");

    lcd.setCursor(8, 0);
    lcd.print("OUT:");
    lcd.print(outputWaterPercent);
    lcd.print("%   ");

    lcd.setCursor(0, 1);
    lcd.print("        ");
    lcd.setCursor(0, 1);
    lcd.print(getWaterLevelName(inputWaterPercent));

    lcd.setCursor(8, 1);
    lcd.print("        ");
    lcd.setCursor(8, 1);
    lcd.print(getWaterLevelName(outputWaterPercent));
}