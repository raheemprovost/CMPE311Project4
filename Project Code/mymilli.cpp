#include "mymilli.h"
#include <stdint.h>      // standard integer types only; no avr/io.h or avr/interrupt.h needed

// ===== Own register constants (ATmega328P data-memory addresses) =====
#define myTCCR0A (*(volatile uint8_t *)0x44)   // Timer/Counter0 Control Register A (mode select)
#define myTCCR0B (*(volatile uint8_t *)0x45)   // Timer/Counter0 Control Register B (clock select)
#define myOCR0A  (*(volatile uint8_t *)0x47)   // Timer/Counter0 Output Compare Register A (compare value)
#define myTIMSK0 (*(volatile uint8_t *)0x6E)   // Timer/Counter0 Interrupt Mask Register
#define mySREG   (*(volatile uint8_t *)0x5F)   // CPU status register (holds the global interrupt enable bit)
#define clksel   0                             // bit position of the clock select field (CS02:0) in myTCCR0B
#define myOCIE0A 1                             // bit position of the compare match A interrupt enable in myTIMSK0

// ===== Own interrupt macros (replace ISR(), cli(), sei() and the vector name) =====
#define myTIMER0_COMPA_vect __vector_14        // interrupt vector for Timer/Counter0 compare match A
#define myISR(vec) extern "C" void vec(void) __attribute__((signal, used, externally_visible)); void vec(void)
#define myCLI() __asm__ __volatile__("cli" ::: "memory")   // disable interrupts
#define mySEI() __asm__ __volatile__("sei" ::: "memory")   // enable interrupts

volatile uint32_t millicount = 0;   // milliseconds since start; written only by the ISR

// Runs once per millisecond when Timer/Counter0 matches OCR0A.
myISR(myTIMER0_COMPA_vect) {
  millicount++;                     // one tick = exactly 1 ms
}

// Sets up Timer/Counter0 to interrupt once per millisecond.
// 16 MHz / 64 = 250 kHz (4 us per count); counting 0 to 249 is 250 counts = 1000 us = 1 ms.
void mymilliInit() {
  myTCCR0A = (0b10 << 0);           // WGM01:0 = 0b10, CTC mode (timer clears on match with OCR0A)
  myOCR0A = 0b11111001;             // compare value 249
  myTIMSK0 = (0b1 << myOCIE0A);     // enable the compare match A interrupt (only this one)
  mySEI();                          // enable interrupts globally
  myTCCR0B = (0b011 << clksel);     // CS02:0 = 0b011, clock divided by 64
}

// Replacement for millis(): returns milliseconds since mymilliInit().
unsigned long mymilli() {
  uint32_t copy;
  uint8_t saved = mySREG;           // remember interrupt state
  myCLI();                          // block the ISR briefly
  copy = millicount;                // 4-byte read can't be split now
  mySREG = saved;                   // restore the previous state
  return copy;
}