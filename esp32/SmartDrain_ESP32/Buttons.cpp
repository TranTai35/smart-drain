#include <Arduino.h>

#include "Config.h"
#include "Pump.h"
#include "Buttons.h"

struct DebouncedButton
{
    int pin;
    int lastReading;
    int stableState;
    unsigned long lastChangeTime;
};

DebouncedButton modeButton;
DebouncedButton pumpButton;

void initializeButton(
    DebouncedButton& button,
    int pin)
{
    button.pin = pin;
    button.lastReading = HIGH;
    button.stableState = HIGH;
    button.lastChangeTime = 0;

    pinMode(pin, INPUT_PULLUP);
}

bool wasButtonPressed(DebouncedButton& button)
{
    int currentReading = digitalRead(button.pin);

    if (currentReading != button.lastReading)
    {
        button.lastChangeTime = millis();
        button.lastReading = currentReading;
    }

    if (millis() - button.lastChangeTime >=
        BUTTON_DEBOUNCE_MS)
    {
        if (currentReading != button.stableState)
        {
            button.stableState = currentReading;

            // INPUT_PULLUP: LOW nghĩa là đang nhấn
            if (button.stableState == LOW)
            {
                return true;
            }
        }
    }

    return false;
}

void setupButtons()
{
    initializeButton(modeButton, MODE_BUTTON_PIN);
    initializeButton(pumpButton, PUMP_BUTTON_PIN);
}

void updateButtons()
{
    if (wasButtonPressed(modeButton))
    {
        toggleOperationMode();
    }

    if (wasButtonPressed(pumpButton))
    {
        toggleManualPump();
    }
}