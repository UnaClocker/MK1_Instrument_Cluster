#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>

// U8G2_R2 flips the display 180 degrees
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);

// Initialize the DS3231 RTC
RTC_DS3231 rtc;

// Hardware Pins
const int BTN_HOUR = 2;
const int BTN_MIN = 3;
const int PIN_DIM = 4; // NEW: Low = Dim (1), High = Bright (255)

// Thermistor Parameters
const int THERMISTOR_PIN = A0;
const float SERIES_RESISTOR = 10000.0; 
const float NOMINAL_RESISTANCE = 10000.0; 
const float NOMINAL_TEMPERATURE = 25.0; 
const float B_COEFFICIENT = 3950.0; 
const float ADC_MAX = 1023.0;

// Timing Variables
unsigned long lastTempRead = 0;
const unsigned long tempInterval = 10000; // Read temp every 10 seconds
float currentTempF = 0.0;

// Time Modification Variables
bool isSettingTime = false;
unsigned long lastButtonPressTime = 0;
const unsigned long saveTimeout = 10000; 

// In-memory clock state for editing
int editHour = 0;
int editMinute = 0;
int editSecond = 0;
int editYear = 2026;
int editMonth = 7;
int editDay = 14;

// Simple state tracking for debouncing buttons and dimming
bool lastHourState = HIGH;
bool lastMinState = HIGH;
int currentBrightness = 255;

void setup() {
  pinMode(BTN_HOUR, INPUT_PULLUP);
  pinMode(BTN_MIN, INPUT_PULLUP);
  
  // NEW: Configure dimming pin with an internal pull-up.
  // If left unconnected, it stays HIGH (bright). Grounding this pin triggers dim state.
  pinMode(PIN_DIM, INPUT_PULLUP);

  u8g2.begin();

  Wire.setClock(100000);

  // Show the VW Splash Screen for 3 seconds on startup
  showVWSplashScreen();
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Handle Dimming Input
  // If PIN_DIM is pulled LOW (active), dim the screen to 1. Otherwise, keep it at 255.
  int targetBrightness = (digitalRead(PIN_DIM) == LOW) ? 1 : 255;
  if (targetBrightness != currentBrightness) {
    currentBrightness = targetBrightness;
    if (targetBrightness==255) {
      setNormalBright();
    } else {
      setUltraDim();
    }
  }

  // 2. Read temperature at defined intervals
  if (currentMillis - lastTempRead >= tempInterval || lastTempRead == 0) {
    lastTempRead = currentMillis;
    currentTempF = readTemperatureF();
  }

  // 3. Fetch the current time
  int displayHour = 0;
  int displayMinute = 0;

  if (!isSettingTime) {
    DateTime now = rtc.now();
    displayHour = now.hour();
    displayMinute = now.minute();
  } else {
    displayHour = editHour;
    displayMinute = editMinute;

    if (currentMillis - lastButtonPressTime >= saveTimeout) {
      rtc.adjust(DateTime(editYear, editMonth, editDay, editHour, editMinute, 0));
      isSettingTime = false;
    }
  }

  // 4. Scan Buttons (Active Low - pressed is LOW)
  bool currentHourState = digitalRead(BTN_HOUR);
  bool currentMinState = digitalRead(BTN_MIN);

  if (currentHourState == LOW && lastHourState == HIGH) {
    delay(50); 
    if (digitalRead(BTN_HOUR) == LOW) {
      if (!isSettingTime) {
        enterEditMode();
      }
      editHour = (editHour + 1) % 24; 
      lastButtonPressTime = currentMillis;
    }
  }

  if (currentMinState == LOW && lastMinState == HIGH) {
    delay(50); 
    if (digitalRead(BTN_MIN) == LOW) {
      if (!isSettingTime) {
        enterEditMode();
      }
      editMinute = (editMinute + 1) % 60; 
      lastButtonPressTime = currentMillis;
    }
  }

  lastHourState = currentHourState;
  lastMinState = currentMinState;

  // 5. Render Layout (Page Buffer Mode)
  u8g2.clearBuffer();
  
    // ----------------------------------------------------
    // Draw Corner Temperature (Top-Right)
    // ----------------------------------------------------
    u8g2.setFont(u8g2_font_7x14_tr); // Large 14px font
    char tempBuffer[8];
    dtostrf(currentTempF, 2, 0, tempBuffer);
    strcat(tempBuffer, "F");
    
    u8g2.drawStr(104, 13, tempBuffer);
    u8g2.drawCircle(98, 5, 1); // Aligned degree symbol

    // If setting time, draw an indicator on the screen
    if (isSettingTime) {
      u8g2.setFont(u8g2_font_5x7_tr); 
      u8g2.drawStr(4, 8, "SET TIME");
    }

    // ----------------------------------------------------
    // Draw Centered Time (HH:MM)
    // ----------------------------------------------------
    u8g2.setFont(u8g2_font_logisoso42_tn);
    char timeBuffer[6];
    
    int amPmHour = displayHour;
    if (amPmHour == 0) {
      amPmHour = 12;
    } else if (amPmHour > 12) {
      amPmHour -= 12;
    }

    sprintf(timeBuffer, "%d:%02d", amPmHour, displayMinute);

    int textWidth = u8g2.getStrWidth(timeBuffer);
    int xPos = (128 - textWidth) / 2;
    int yPos = 60; // Perfectly aligned to the bottom edge

    u8g2.drawStr(xPos, yPos, timeBuffer);

  u8g2.sendBuffer();
  delay(10); 
}

// Renders a perfectly proportioned vector VW logo centered on the 128x64 display
void showVWSplashScreen() {
  const int centerX = 64;
  const int centerY = 32;
  const int outerRadius = 28;

  u8g2.clearBuffer(); // Clear the full buffer once

  // Draw outer double-ring emblem
  u8g2.drawCircle(centerX, centerY, outerRadius);
  u8g2.drawCircle(centerX, centerY, outerRadius - 2);

  // ----------------------------------------------------
  // --- "V" Section (Strictly Top Half: Y: 10 to 30) ---
  // ----------------------------------------------------
  u8g2.drawLine(centerX - 12, centerY - 20, centerX - 2, centerY - 2);
  u8g2.drawLine(centerX - 11, centerY - 20, centerX - 1, centerY - 2);
  u8g2.drawLine(centerX + 12, centerY - 20, centerX + 2, centerY - 2);
  u8g2.drawLine(centerX + 11, centerY - 20, centerX + 1, centerY - 2);

  // ----------------------------------------------------
  // --- Horizontal Split (Gap between V and W) ---------
  // ----------------------------------------------------
  u8g2.setDrawColor(0);
  u8g2.drawHLine(centerX - 24, centerY - 1, 48);
  u8g2.drawHLine(centerX - 24, centerY, 48);
  u8g2.setDrawColor(1);

  // ----------------------------------------------------
  // --- "W" Section (Strictly Bottom Half: Y: 33 to 53) ---
  // ----------------------------------------------------
  u8g2.drawLine(centerX - 18, centerY + 2, centerX - 9, centerY + 20);
  u8g2.drawLine(centerX - 17, centerY + 2, centerX - 8, centerY + 20);
  u8g2.drawLine(centerX - 9, centerY + 20, centerX - 1, centerY + 2);
  u8g2.drawLine(centerX - 8, centerY + 20, centerX - 1, centerY + 3);
  u8g2.drawLine(centerX + 9, centerY + 20, centerX + 1, centerY + 2);
  u8g2.drawLine(centerX + 8, centerY + 20, centerX + 1, centerY + 3);
  u8g2.drawLine(centerX + 18, centerY + 2, centerX + 9, centerY + 20);
  u8g2.drawLine(centerX + 17, centerY + 2, centerX + 8, centerY + 20);

  u8g2.sendBuffer(); // Push the whole logo to the screen at once

  // Hold the logo on the screen for 3000ms (3 seconds)
  delay(3000);
}

void enterEditMode() {
  DateTime now = rtc.now();
  editHour = now.hour();
  editMinute = now.minute();
  editSecond = 0;
  editYear = now.year();
  editMonth = now.month();
  editDay = now.day();
  isSettingTime = true;
}

float readTemperatureF() {
  int analogVal = analogRead(THERMISTOR_PIN);
  if (analogVal == 0) return 0.0;
  
  float resistance = ADC_MAX / (float)analogVal - 1.0;
  resistance = SERIES_RESISTOR / resistance;
  
  float steinhart;
  steinhart = resistance / NOMINAL_RESISTANCE;     
  steinhart = log(steinhart);                      
  steinhart /= B_COEFFICIENT;                      
  steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15); 
  steinhart = 1.0 / steinhart;                     
  steinhart -= 273.15;                             
  
  return (steinhart * 9.0 / 5.0) + 32.0;
}

void setUltraDim() {
  u8g2.setContrast(1); // Drop master contrast to minimum
  
  // Send raw commands to the SSD1306 controller
  u8g2.sendF("c", 0xD9); // Set Pre-Charge Period
  u8g2.sendF("c", 0x22); // Minimum pre-charge phase (Default is usually 0x22 or 0xF1)
  
  u8g2.sendF("c", 0xDB); // Set VCOMH Deselect Level
  u8g2.sendF("c", 0x10); // Set to lowest voltage level (0.65 x Vcc)
}

void setNormalBright() {
  u8g2.setContrast(255); // Restore master contrast
  
  // Restore default pre-charge and VCOMH levels
  u8g2.sendF("c", 0xD9);
  u8g2.sendF("c", 0xF1); 
  
  u8g2.sendF("c", 0xDB);
  u8g2.sendF("c", 0x20); 
}