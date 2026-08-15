#include <Arduino.h>
#include <WiFi.h>

#include "Config.h"
#include "NetworkManager.h"

const unsigned long WIFI_RETRY_INTERVAL = 10000UL;

unsigned long lastWiFiAttempt = 0;
bool wifiConnectedAnnounced = false;

void startWiFiConnection()
{
    Serial.print("[WIFI] Dang ket noi toi: ");
    Serial.println(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    lastWiFiAttempt = millis();
}

void setupNetwork()
{
    WiFi.mode(WIFI_STA);

    // Không lưu cấu hình cũ vào bộ nhớ flash
    WiFi.persistent(false);

    startWiFiConnection();
}

void updateNetwork()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        if (!wifiConnectedAnnounced)
        {
            wifiConnectedAnnounced = true;

            Serial.println("[WIFI] Ket noi thanh cong");

            Serial.print("[WIFI] IP: ");
            Serial.println(WiFi.localIP());

            Serial.print("[WIFI] Signal: ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
        }

        return;
    }

    if (wifiConnectedAnnounced)
    {
        wifiConnectedAnnounced = false;
        Serial.println("[WIFI] Mat ket noi");
    }

    unsigned long currentTime = millis();

    if (currentTime - lastWiFiAttempt >=
        WIFI_RETRY_INTERVAL)
    {
        Serial.println("[WIFI] Dang thu ket noi lai");

        WiFi.disconnect();
        startWiFiConnection();
    }
}

bool isWiFiConnected()
{
    return WiFi.status() == WL_CONNECTED;
}