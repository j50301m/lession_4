#include <Arduino.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>

// Initalize the LCD
#define I2C_ADDR    0x27
#define LCD_COLUMNS 16
#define LCD_ROWS    2
LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_ROWS);

// Initalize the RTC
RTC_DS1307 rtc;

#define SUNMIT_BTN 6
#define UP_BTN 5
#define DOWN_BTN 4
#define BACK_BTN 3

void printWithLeadingZeros(int number, unsigned int totalDigits);
void printWithBlinking(int number, unsigned int totalDigits, bool shouldBlink);
void displayTime();
void handleSetTime();
void renderSetTimeScreen();
int daysInMonth(int year, int month);

// mode: 0 = display time, 1 = set time
int mode = 0;

// Temporary variables for setting mode
int setYear, setMonth, setDay, setHour, setMinute, setSecond;
int setFieldIndex = 0; // 0: Year, 1: Month, 2: Day, 3: Hour, 4: Minute
bool setModeInitialized = false;

// ---------- Non-blocking button debouncing ----------
const unsigned long DEBOUNCE_MS = 50;

bool submitStable = HIGH, submitLastReading = HIGH;
bool upStable = HIGH, upLastReading = HIGH;
bool downStable = HIGH, downLastReading = HIGH;
bool backStable = HIGH, backLastReading = HIGH;

unsigned long submitLastChange = 0;
unsigned long upLastChange = 0;
unsigned long downLastChange = 0;
unsigned long backLastChange = 0;

// Button "press event" (HIGH->LOW) in each loop cycle
bool submitPressed = false;
bool upPressed = false;
bool downPressed = false;
bool backPressed = false;

// ---------- Display update frequency ----------
const unsigned long DISPLAY_INTERVAL_MS = 20;
unsigned long lastDisplayUpdate = 0;

// ---------- Blinking control for set mode ----------
const unsigned long BLINK_INTERVAL_MS = 500;
unsigned long lastBlinkUpdate = 0;
bool blinkState = true; // true = show field, false = hide field


void setup() {
  Serial.begin(9600);

  // Start the RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    abort();
  }

  // Start the LCD
  lcd.init();
  lcd.backlight();

  // Set pin modes for buttons
  pinMode(SUNMIT_BTN, INPUT_PULLUP);
  pinMode(UP_BTN, INPUT_PULLUP);
  pinMode(DOWN_BTN, INPUT_PULLUP);
  pinMode(BACK_BTN, INPUT_PULLUP);
}

void updateButton(int pin,
                  bool &stableState, bool &lastReading,
                  unsigned long &lastChange,
                  bool &pressedEvent,
                  unsigned long nowMs) {
  bool reading = digitalRead(pin);

  // If reading changes, update "last change time"
  if (reading != lastReading) {
    lastReading = reading;
    lastChange = nowMs;
  }

  // After debounce time and stable state actually changes, update stableState
  if ((nowMs - lastChange) > DEBOUNCE_MS && reading != stableState) {
    stableState = reading;
    // INPUT_PULLUP: LOW = pressed
    if (stableState == LOW) {
      pressedEvent = true; // One "press event" in this loop
    }
  }
}

void loop() {
  unsigned long nowMs = millis();

  // Reset "press events" at the beginning of each loop
  submitPressed = upPressed = downPressed = backPressed = false;

  // Update debounce state for all buttons & whether pressedEvent is generated
  updateButton(SUNMIT_BTN, submitStable, submitLastReading, submitLastChange, submitPressed, nowMs);
  updateButton(UP_BTN,     upStable,     upLastReading,     upLastChange,     upPressed,     nowMs);
  updateButton(DOWN_BTN,   downStable,   downLastReading,   downLastChange,   downPressed,   nowMs);
  updateButton(BACK_BTN,   backStable,   backLastReading,   backLastChange,   backPressed,   nowMs);

  // Mode switching logic (display -> set)
  if (mode == 0 && submitPressed) {
    mode = 1;
    setModeInitialized = false;
    lcd.clear();
    return;
  }

  // Handle logic based on mode (don't draw screen here, screen drawing is unified below with frequency control)
  switch (mode) {
    case 0:
      // Display mode: logic is simple, nothing to do
      break;
    case 1:
      handleSetTime();
      break;
    default:
      break;
  }

  // Control blink state for set mode (non-blocking)
  if (mode == 1 && (nowMs - lastBlinkUpdate >= BLINK_INTERVAL_MS)) {
    lastBlinkUpdate = nowMs;
    blinkState = !blinkState;
  }

  // Control screen update frequency (non-blocking)
  if (nowMs - lastDisplayUpdate >= DISPLAY_INTERVAL_MS) {
    lastDisplayUpdate = nowMs;

    if (mode == 0) {
      displayTime();
    } else if (mode == 1) {
      renderSetTimeScreen();
    }
  }
}

// ------------------ Display Time ------------------
void displayTime() {
  DateTime now = rtc.now();

  // lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Day: ");
  printWithLeadingZeros(now.year(), 4);
  lcd.print('/');
  printWithLeadingZeros(now.month(), 2);
  lcd.print('/');
  printWithLeadingZeros(now.day(), 2);

  lcd.setCursor(0, 1);
  lcd.print("Time: ");
  printWithLeadingZeros(now.hour(), 2);
  lcd.print(':');
  printWithLeadingZeros(now.minute(), 2);
  lcd.print(':');
  printWithLeadingZeros(now.second(), 2);
  lcd.print("  ");
}

// ------------------ Set Time Logic (only handle "value change logic", no screen drawing) ------------------
void handleSetTime() {
  // First time entering set mode, read current RTC time as default values
  if (!setModeInitialized) {
    DateTime now = rtc.now();
    setYear   = now.year();
    setMonth  = now.month();
    setDay    = now.day();
    setHour   = now.hour();
    setMinute = now.minute();
    setSecond = now.second();
    setFieldIndex = 0;
    setModeInitialized = true;
  }

  // BACK: abandon changes, return directly to display mode
  if (backPressed) {
    mode = 0;
    return;
  }

  // UP / DOWN: adjust current field
  if (upPressed) {
    switch (setFieldIndex) {
      case 0: // Year
        if (setYear < 2099) setYear++;
        break;
      case 1: // Month
        setMonth++;
        if (setMonth > 12) setMonth = 1;
        break;
      case 2: { // Day
        int dim = daysInMonth(setYear, setMonth);
        setDay++;
        if (setDay > dim) setDay = 1;
        break;
      }
      case 3: // Hour
        setHour++;
        if (setHour > 23) setHour = 0;
        break;
      case 4: // Minute
        setMinute++;
        if (setMinute > 59) setMinute = 0;
        break;
    }
  }

  if (downPressed) {
    switch (setFieldIndex) {
      case 0: // Year
        if (setYear > 2000) setYear--;
        break;
      case 1: // Month
        setMonth--;
        if (setMonth < 1) setMonth = 12;
        break;
      case 2: { // Day
        int dim = daysInMonth(setYear, setMonth);
        setDay--;
        if (setDay < 1) setDay = dim;
        break;
      }
      case 3: // Hour
        setHour--;
        if (setHour < 0) setHour = 23;
        break;
      case 4: // Minute
        setMinute--;
        if (setMinute < 0) setMinute = 59;
        break;
    }
  }

  // SUBMIT: switch to next field; save time and return to display mode on last field
  if (submitPressed) {
    setFieldIndex++;
    if (setFieldIndex > 4) {
      // Write back to RTC
      rtc.adjust(DateTime(setYear, setMonth, setDay, setHour, setMinute, 0));
      mode = 0; // Return to display mode
    }

  }
}

// ------------------ Set Time Screen Rendering ------------------
void renderSetTimeScreen() {
  // lcd.clear();
  lcd.setCursor(0, 0);

  // Display which field is currently being set
  switch (setFieldIndex) {
    case 0: lcd.print("Set Year   "); break;
    case 1: lcd.print("Set Month  "); break;
    case 2: lcd.print("Set Day    "); break;
    case 3: lcd.print("Set Hour   "); break;
    case 4: lcd.print("Set Minute "); break;
  }

  // Second line displays complete date and time
  lcd.setCursor(0, 1);
  printWithBlinking(setYear, 4, setFieldIndex == 0);
  lcd.print('/');
  printWithBlinking(setMonth, 2, setFieldIndex == 1);
  lcd.print('/');
  printWithBlinking(setDay, 2, setFieldIndex == 2);
  lcd.print(' ');
  printWithBlinking(setHour, 2, setFieldIndex == 3);
  lcd.print(':');
  printWithBlinking(setMinute, 2, setFieldIndex == 4);
}

// ------------------ Utility Functions ------------------
void printWithBlinking(int number, unsigned int totalDigits, bool shouldBlink) {
  if (shouldBlink && !blinkState) {
    // Print spaces instead of numbers when blinking off
    for (unsigned int i = 0; i < totalDigits; i++) {
      lcd.print(" ");
    }
  } else {
    printWithLeadingZeros(number, totalDigits);
  }
}

void printWithLeadingZeros(int number, unsigned int totalDigits) {
  String numStr = String(number);
  while (numStr.length() < totalDigits) {
    numStr = "0" + numStr;
  }
  lcd.print(numStr);
}



int daysInMonth(int year, int month) {
  static const int days[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
  int d = days[month - 1];
  bool isLeap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
  if (month == 2 && isLeap) d = 29;
  return d;
}
