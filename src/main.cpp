#if defined(ARDUINO)

#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    Serial.println("BMW E9x Remote Control: safe inert firmware");
    Serial.println("No vehicle or actuator adapter is configured.");
}

void loop() {
    delay(1'000U);
}

#else

int main() {
    // The native firmware target intentionally performs no physical action.
    // Behavioral validation is implemented in tests/test_main.cpp.
    return 0;
}

#endif
