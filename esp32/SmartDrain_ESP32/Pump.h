#ifndef PUMP_H
#define PUMP_H

extern bool pumpRunning;

void setupPump();
void updatePump();
void setPump(bool state);

#endif