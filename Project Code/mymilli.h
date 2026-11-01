#ifndef mymilli_H   // include guard: skip this file if it was already included
#define mymilli_H

// Sets up Timer0 to interrupt once per millisecond (CTC mode, prescaler 64, OCR0A = 249).
// Call once in setup() before using mymilli() (fanInit()'s button debounce depends on it).
void mymilliInit(void);

// Returns the milliseconds elapsed since mymilliInit() was called.
// Replacement for Arduino's millis(); the count is kept by the Timer0 compare-match ISR.
unsigned long mymilli(void);

#endif  // mymilli_H