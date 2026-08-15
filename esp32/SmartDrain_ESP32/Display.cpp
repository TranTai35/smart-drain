#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <stdio.h>

#include "Config.h"
#include "WaterSensor.h"
#include "Pump.h"
#include "AlertSystem.h"
#include "Display.h"

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// ============================================================
// CÁC TRANG HIỂN THỊ
// ============================================================
//
// Mục 3.1 báo cáo yêu cầu LCD hiển thị mức nước hai bể, chế độ, trạng thái
// máy bơm và cảnh báo. Màn 16x2 chỉ có 32 ký tự, không đủ chỗ cho tất cả,
// nên các trang được hiển thị luân phiên.
//
// Trang mức nước hiển thị LÂU HƠN HẲN hai trang còn lại. Đây là thông tin
// người xem cần theo dõi liên tục, còn chế độ và cảnh báo chỉ cần liếc qua
// là nắm được.
//
// Trang cảnh báo chỉ xuất hiện khi thực sự có cảnh báo. Lúc hệ thống bình
// thường thì chỉ xoay vòng giữa hai trang đầu.

enum DisplayPage
{
    PAGE_WATER,
    PAGE_STATUS,
    PAGE_ALERT
};

const unsigned long PAGE_WATER_MS = 5000UL;
const unsigned long PAGE_STATUS_MS = 2000UL;
const unsigned long PAGE_ALERT_MS = 2500UL;

DisplayPage currentPage = PAGE_WATER;
unsigned long pageStartTime = 0;

// ============================================================
// IN MỘT DÒNG
// ============================================================

// LCD không tự xóa ký tự cũ: in chuỗi ngắn đè lên chuỗi dài sẽ để lại phần
// đuôi của chuỗi trước. Vì vậy mọi dòng đều được đệm khoảng trắng cho đủ
// đúng 16 ký tự trước khi in.
void printLine(int row, const char* text)
{
    char buffer[LCD_COLUMNS + 1];

    snprintf(
        buffer,
        sizeof(buffer),
        "%-*s",
        LCD_COLUMNS,
        text
    );

    lcd.setCursor(0, row);
    lcd.print(buffer);
}

// ============================================================
// SETUP
// ============================================================

void setupDisplay()
{
    Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);

    lcd.init();
    lcd.backlight();
    lcd.clear();

    printLine(0, "SMART DRAIN");
    printLine(1, "Starting...");

    delay(1000);
    lcd.clear();

    currentPage = PAGE_WATER;
    pageStartTime = millis();
}

// ============================================================
// TỪNG TRANG
// ============================================================

// IN:72%  OUT:45%
// HIGH    MEDIUM
void showWaterPage()
{
    char line[LCD_COLUMNS + 1];

    snprintf(
        line,
        sizeof(line),
        "IN:%d%% OUT:%d%%",
        inputWaterPercent,
        outputWaterPercent
    );
    printLine(0, line);

    snprintf(
        line,
        sizeof(line),
        "%-8s%s",
        getWaterLevelName(inputWaterPercent),
        getWaterLevelName(outputWaterPercent)
    );
    printLine(1, line);
}

// Mode: AUTO
// Pump: ON   15s
void showStatusPage()
{
    char line[LCD_COLUMNS + 1];

    snprintf(
        line,
        sizeof(line),
        "Mode: %s",
        getOperationModeName()
    );
    printLine(0, line);

    if (pumpRunning)
    {
        unsigned long seconds =
            getPumpRuntimeMs() / 1000UL;

        // Thực tế không bao giờ vượt max_runtime (tối đa 600 giây), nhưng vẫn
        // chặn lại để chuỗi luôn vừa 16 ký tự dù có chuyện gì xảy ra
        if (seconds > 9999UL)
        {
            seconds = 9999UL;
        }

        snprintf(
            line,
            sizeof(line),
            "Pump: ON %us",
            (unsigned int)seconds
        );
    }
    else
    {
        snprintf(
            line,
            sizeof(line),
            "Pump: OFF"
        );
    }
    printLine(1, line);
}

// ! CANH BAO
// Ong bi nghet?
void showAlertPage()
{
    printLine(0, "! CANH BAO");
    printLine(1, getAlertShortText());
}

// ============================================================
// CHUYỂN TRANG
// ============================================================

unsigned long currentPageDuration()
{
    switch (currentPage)
    {
        case PAGE_STATUS:
            return PAGE_STATUS_MS;

        case PAGE_ALERT:
            return PAGE_ALERT_MS;

        default:
            return PAGE_WATER_MS;
    }
}

DisplayPage nextPage()
{
    switch (currentPage)
    {
        case PAGE_WATER:
            return PAGE_STATUS;

        // Bỏ qua trang cảnh báo khi hệ thống bình thường
        case PAGE_STATUS:
            return isAlertActive()
                ? PAGE_ALERT
                : PAGE_WATER;

        default:
            return PAGE_WATER;
    }
}

// ============================================================
// UPDATE
// ============================================================

bool lastAlertActive = false;

void updateDisplay()
{
    unsigned long currentTime = millis();
    bool alertActive = isAlertActive();

    // Cảnh báo vừa xuất hiện thì nhảy sang trang cảnh báo ngay, không bắt
    // người dùng đợi hết lượt của trang đang hiển thị
    if (alertActive && !lastAlertActive)
    {
        currentPage = PAGE_ALERT;
        pageStartTime = currentTime;
    }
    // Không dùng delay() để đổi trang: delay() sẽ chặn mqtt.loop() và làm
    // ESP32 bị broker ngắt kết nối vì quá keep-alive.
    else if (currentTime - pageStartTime >=
             currentPageDuration())
    {
        pageStartTime = currentTime;
        currentPage = nextPage();
    }

    lastAlertActive = alertActive;

    switch (currentPage)
    {
        case PAGE_STATUS:
            showStatusPage();
            break;

        case PAGE_ALERT:
            showAlertPage();
            break;

        default:
            showWaterPage();
            break;
    }
}
