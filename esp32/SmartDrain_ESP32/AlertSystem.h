#ifndef ALERT_SYSTEM_H
#define ALERT_SYSTEM_H

enum AlertLevel
{
    ALERT_NORMAL,
    ALERT_HIGH,
    ALERT_DANGER
};

extern AlertLevel currentAlertLevel;

void setupAlertSystem();
void updateAlertSystem();

void setBuzzerMuted(bool muted);
bool isBuzzerMuted();

const char* getAlertLevelName();

#endif