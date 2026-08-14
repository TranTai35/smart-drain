#ifndef PUMP_H
#define PUMP_H

enum OperationMode
{
    MODE_AUTO,
    MODE_MANUAL
};

extern bool pumpRunning;
extern OperationMode operationMode;

void setupPump();

void setPump(bool state);
void setOperationMode(OperationMode newMode);

void toggleOperationMode();
void toggleManualPump();

void updatePump();

const char* getOperationModeName();

#endif