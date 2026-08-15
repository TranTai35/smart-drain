#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

void setupMqttManager();
void updateMqttManager();

bool isMqttConnected();

void publishTankData();
void publishPumpState();
void publishMode();
void publishAlert();
void publishConfig();

#endif