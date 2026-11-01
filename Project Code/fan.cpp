#include "fan.h"
#include "button.h"
#include <stdint.h>      // standard integer types only; no avr/io.h or avr/interrupt.h needed

// ===== Own register constants (ATmega328P data-memory addresses) =====
#define myDDRB (*(volatile uint8_t *)0x24)      // Port B Data Direction Register: fan = output
#define myTCCR1A (*(volatile uint8_t *)0x80)    // Timer/Counter1 Control Register A (COM1A/COM1B output mode, WGM11:10)
#define myTCCR1B (*(volatile uint8_t *)0x81)    // Timer/Counter1 Control Register B (WGM13:12, clock select CS12:0)
#define myOCR1A  (*(volatile uint16_t *)0x88)   // Timer/Counter1 Output Compare Register A (16-bit, duty compare for OC1A)
#define myICR1  (*(volatile uint16_t *)0x86)    // Timer/Counter1 Input Capture Register (16-bit, used as TOP in mode 14)
#define clksel   0                              // bit position of the clock select field (CS12:0) in myTCCR1B

// ===== Pin assignment =====
#define FAN_BIT     1    // PB1 = Arduino pin 9 = OC1A, PWM output to the MOSFET gate (through the gate resistor)

// ===== Fan duty compare values (duty fraction x (TOP + 1), TOP = 249) =====
#define DUTY_OFF  0      // 0%: fan off
#define DUTY_LOW  100    // 40%: 100 / 250 = 0.40
#define DUTY_MED  160    // 64%: 160 / 250 = 0.64
#define DUTY_HIGH 249    // 100%: OCR1A = TOP, so the output stays high (no compare match)

// Fan speed states, in the order they are first reached
typedef enum {
  STATE_OFF,
  STATE_LOW,
  STATE_MED,
  STATE_HIGH
} buttonState;

static buttonState newState = STATE_OFF;     // current fan state (holds the state the fan is in now)
static buttonState prevState = STATE_OFF;    // state we came from; tells MED and LOW whether we are going up or down

// Fan state machine: OFF -> LOW -> MED -> HIGH -> MED -> LOW -> OFF, one step per button release.
// Called repeatedly from the task table in the .ino.
void lightUpdate() {
  uint8_t buttonData = checkbuttonReleased();   // read the button once per call so every case sees the same event
  switch (newState) {
    case STATE_OFF:
      if (buttonData) {                 // release while off: start going up
        newState = STATE_LOW;
        myOCR1A = DUTY_LOW;
        prevState = STATE_OFF;
      }
      break;

    case STATE_LOW:
      if (buttonData && (prevState == STATE_OFF)) {          // came from OFF: going up
        newState = STATE_MED;
        myOCR1A = DUTY_MED;
        prevState = STATE_LOW;
      } else if (buttonData && (prevState == STATE_MED)) {   // came from MED: going down
        newState = STATE_OFF;
        myOCR1A = DUTY_OFF;
        prevState = STATE_LOW;
      }
      break;

    case STATE_MED:
      if (buttonData && (prevState == STATE_LOW)) {          // came from LOW: going up
        newState = STATE_HIGH;
        myOCR1A = DUTY_HIGH;
        prevState = STATE_MED;
      } else if (buttonData && (prevState == STATE_HIGH)) {  // came from HIGH: going down
        newState = STATE_LOW;
        myOCR1A = DUTY_LOW;
        prevState = STATE_MED;
      }
      break;

    case STATE_HIGH:
      if (buttonData && (prevState == STATE_MED)) {          // top of the cycle: start going down
        newState = STATE_MED;
        myOCR1A = DUTY_MED;
        prevState = STATE_HIGH;
      }
      break;
  }
}

// Sets up Timer/Counter1 for Fast PWM (mode 14) on OC1A (Arduino pin 9, PB1).
// 16 MHz / 8 = 2 MHz (0.5 us per count); counting 0 to 249 is 250 counts = 125 us = 8 kHz PWM.
void fanInit() {
  myDDRB |= (1 << FAN_BIT);         // fan pin (OC1A) as output for the fan MOSFET gate
  myTCCR1A = 0;   // clear
  myTCCR1B = 0;   // clear
  myTCCR1A |= (0b10 << 0) | (0b10 << 6);   // WGM11:10 = 0b10 (low half of mode 14); COM1A1:0 = 0b10 (non-inverting: clear OC1A on compare match, set at BOTTOM)
  myICR1 = 0b0000000011111001;        // TOP = 249 (0x00F9): 250 counts per period, 2 MHz / 250 = 8 kHz PWM
  myOCR1A = DUTY_OFF;                 // start with the fan off
  myTCCR1B |= (0b11 << 3) | (0b010 << clksel);     // WGM13:12 = 0b11 (high half of mode 14, Fast PWM with TOP = ICR1); CS12:0 = 0b010 = divide by 8 (2 MHz timer clock); written last so the timer starts only after setup
}