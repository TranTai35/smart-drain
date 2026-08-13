#ifndef WATER_SENSOR_H
#define WATER_SENSOR_H

extern int inputWaterADC;
extern int outputWaterADC;

extern int inputWaterPercent;
extern int outputWaterPercent;

void setupWaterSensors();
void updateWaterSensors();

const char* getWaterLevelName(int percent);

#endif