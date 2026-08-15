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

enum PumpSource
{
    PUMP_SOURCE_BOOT,
    PUMP_SOURCE_AUTO,
    PUMP_SOURCE_MANUAL,
    PUMP_SOURCE_SAFETY
};

extern bool pumpRunning;
extern OperationMode operationMode;
extern PumpFault pumpFault;
extern PumpSource pumpSource;

void setupPump();

void setPump(
    bool state,
    PumpSource source = PUMP_SOURCE_MANUAL
);
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
const char* getPumpSourceName();

unsigned long getPumpRuntimeMs();

#endif