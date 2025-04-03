#include "pins.h"

#include <Blinkenlight.h>

#define BLINK_INTERVAL_MS 100
#define CONFIRM_TIME_MS 100
#define MAX_BLINKS 6

Blinkenlight ledR(PIN_LED_R);
Blinkenlight ledG(PIN_LED_G);
Blinkenlight buzzer(PIN_BUZZER);