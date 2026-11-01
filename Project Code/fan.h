#ifndef fan_H // include guard: skip this file if it was already included
#define fan_H

#include <stdint.h>

// Sets up Timer1 for Fast PWM on pin 9 (OC1A) to drive the fan MOSFET gate.
// Call once in setup().
void fanInit(void);

// Fan state machine: reads the button (button.h) and steps OFF -> LOW -> MED -> HIGH -> MED -> LOW -> OFF,
// one step per release, updating the PWM duty (OCR1A). Call repeatedly from loop().
void lightUpdate(void);

#endif  // fan_H