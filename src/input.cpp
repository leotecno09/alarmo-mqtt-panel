#include <Arduino.h>
#include "input.h"

char input_get_key() {
    if (Serial.available()) {
        return Serial.read();
    }

    return '\0';
}