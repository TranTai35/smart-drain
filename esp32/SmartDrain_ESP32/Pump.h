#ifndef PUMP_H
#define PUMP_H

enum OperationMode
{
    MODE_AUTO,
    MODE_MANUAL
};

enum PumpFault
{
    PUMP_FAULT_NONE,
    PUMP_FAULT_TIMEOUT,
    PUMP_FAULT_DRAIN_ABNORMAL
};

extern bool pumpRunning;
extern OperationMode operationMode;
extern PumpFault pumpFault;

void setupPump();

void setPump(bool state);
void setOperationMode(OperationMode newMode);

void toggleOperationMode();
void toggleManualPump();

void updatePump();

void clearPumpFault();
void triggerPumpTimeout();

void triggerDrainAbnormal();
void updateDrainEffectiveness();

const char* getOperationModeName();
const char* getPumpFaultName();

unsigned long getPumpRuntimeMs();

#endif