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
void displayTime();

void setup() {
  // Start the RTC
  if (!rtc.begin()) {
    Serial.begin(9600);
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

int mode = 0; // 0: Display Time, 1: Set Time

void loop() {

  if (digitalRead(SUNMIT_BTN) == LOW) {
    mode = 1;
    delay(300); // Debounce delay
  }


  switch (mode) {
    case 0:
      displayTime();
      break;
    case 1:
      break;
    default:
      break;
  }






}


void displayTime() {
  DateTime now = rtc.now();

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
}


void printWithLeadingZeros(int number, unsigned int totalDigits) {
  String numStr = String(number);
  while (numStr.length() < totalDigits) {
    numStr = "0" + numStr;
  }
  lcd.print(numStr);
}