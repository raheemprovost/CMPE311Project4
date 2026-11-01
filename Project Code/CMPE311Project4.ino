#include "mymilli.h"   // own replacement for millis(): mymilliInit() and mymilli()
#include "fan.h"       // fan module: fanInit() (Timer1 PWM on pin 9) and lightUpdate() (fan state machine)
#include "button.h"    // button module: buttonInit() (pin 10 input with pull-up) and checkbuttonReleased()

// ===== Serial input buffer =====
String inputBuffer = "";  // builds up one line of typed input, character by character

// ===== LED timing state (one set per LED, so they can blink independently) =====
unsigned long currentLED1millis  = 0;
unsigned long currentLED2millis  = 0;
unsigned long previousLED1millis = 0;
unsigned long previousLED2millis = 0;

// ===== Configuration captured from the user =====
int getLED1Num   = 0;   // will be set to 1 once the user picks LED 1
int getLED2Num   = 0;   // will be set to 2 once the user picks LED 2
unsigned long getInterval1 = 0;   // blink interval for LED 1, in ms
unsigned long getInterval2 = 0;   // blink interval for LED 2, in ms

// ===== LED on/off state =====
bool LED1State = false;
bool LED2State = false;

// ===== Question-flow state machine flags =====
bool askedLED      = false;  // have we asked "What LED?" for the current round?
bool askedInterval = false;  // have we asked "What interval?" for the current round?

bool gotLine = true;  // true exactly when a full line of input just finished
int value;            // holds the number parsed from that completed line
int currentSelection = 0;  // which LED THIS round's interval answer belongs to (1 or 2)

typedef void (*TaskFunction)();

void askQuestions();
void serviceLED1();
void serviceLED2();
void checkButton();   // fan task: steps the fan speed on each debounced button release (calls lightUpdate())
void changeFrequency1(int LEDNum, unsigned long LEDInterval);
void changeFrequency2(int LEDNum, unsigned long LEDInterval);
void blinkLED1(unsigned long LEDInterval);
void blinkLED2(unsigned long LEDInterval);
bool serialData(int &result);

// Function pointer array (the schedule)
TaskFunction taskTable[] = {
  askQuestions,
  serviceLED1,
  serviceLED2,
  checkButton      // fan: button polling and fan speed state machine, one call per pass of the schedule
};

const int NUM_TASKS = sizeof(taskTable) / sizeof(taskTable[0]);

void setup() {
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  Serial.begin(9600);
  mymilliInit();   // start the 1 ms timebase on Timer0 (must be running before the button debounce uses mymilli())
  buttonInit();    // button: pin 10 as input with internal pull-up
  fanInit();       // fan: Timer1 Fast PWM on pin 9 (OC1A) for the MOSFET gate
}

void loop() {
  static int currentTask = 0;
  taskTable[currentTask]();
  currentTask = (currentTask + 1) % NUM_TASKS;
}

void askQuestions() {
    // Ask which LED, once per round.
  if (!askedLED) {
    Serial.println("What LED? (1 or 2)");
    askedLED = true;
  }

  // Try to read ONE completed line this pass through loop().
  value = 0;
  gotLine = serialData(value);

  if (gotLine) {
    if (!askedInterval) {
      // We were waiting for the LED number.
      if (value == 1) {
        getLED1Num = value;
        Serial.println("What interval (in msec)?");
        askedInterval = true;
        currentSelection = 1;
      } else if (value == 2) {
        getLED2Num = value;
        Serial.println("What interval (in msec)?");
        askedInterval = true;
        currentSelection = 2;
      }
    } else {
      // We were waiting for the interval — route it to whichever LED was picked.
      if (currentSelection == 1) {
        getInterval1 = value;
      } else if (currentSelection == 2) {
        getInterval2 = value;
      }

      // Reset the flow so the next completed line starts a fresh round
      // (new LED number), instead of being misread as another interval.
      askedLED      = false;
      askedInterval = false;
    }
  }
}

void serviceLED1() {
    // Keep blinking whichever LED(s) have been fully configured.
  if (getLED1Num == 1 && getInterval1 > 0) {
    changeFrequency1(getLED1Num, getInterval1);
  }
}

void serviceLED2() {
  if (getLED2Num == 2 && getInterval2 > 0) {
    changeFrequency2(getLED2Num, getInterval2);
  }
}

// Fan task: polls the button (debounced with mymilli()) and, on each release,
// advances the fan OFF -> LOW -> MED -> HIGH -> MED -> LOW -> OFF.
// Non-blocking, so it does not disturb the LED blinking or the serial dialog.
void checkButton() {
  lightUpdate();
}

// Starts/updates blinking on LED 1, only if LED 1 was the one selected.
void changeFrequency1(int LEDNum, unsigned long LEDInterval) {
  if (LEDNum == 1) {
    blinkLED1(LEDInterval);
  }
}

// Starts/updates blinking on LED 2, only if LED 2 was the one selected.
void changeFrequency2(int LEDNum, unsigned long LEDInterval) {
  if (LEDNum == 2) {
    blinkLED2(LEDInterval);
  }
}

// Non-blocking blink for LED 1 (pin 2), using its own timestamp so it
// doesn't interfere with LED 2's timing.
void blinkLED1(unsigned long LEDInterval) {
  currentLED1millis = mymilli();

  if (currentLED1millis >= previousLED1millis + LEDInterval) {
    LED1State = !LED1State;
    digitalWrite(2, LED1State);
    previousLED1millis = currentLED1millis;
  }
}

// Non-blocking blink for LED 2 (pin 3), using its own timestamp so it
// doesn't interfere with LED 1's timing.
void blinkLED2(unsigned long LEDInterval) {
  currentLED2millis = mymilli();

  if (currentLED2millis >= previousLED2millis + LEDInterval) {
    LED2State = !LED2State;
    digitalWrite(3, LED2State);
    previousLED2millis = currentLED2millis;
  }
}

// Reads at most ONE character per call. Only returns true on the exact
// call where it reads '\n' — at that point the full line is complete,
// so it converts the buffered text to an int and clears the buffer.
bool serialData(int &result) {
  if (Serial.available() > 0) {
    char incomingChar = Serial.read();

    if (incomingChar == '\n') {
      result = inputBuffer.toInt();
      inputBuffer = "";
      return true;
    } else {
      inputBuffer += incomingChar;  // keep building the line
    }
  }
  return false;
}