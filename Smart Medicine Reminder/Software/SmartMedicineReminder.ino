#include <ESP32Servo.h>
#include <Wire.h>
#include "RTClib.h"

// RTC Module (DS3231)
RTC_DS3231 rtc;

// Main Servo (MG995) - Pin 2
const int MAIN_SERVO_PIN = 2;

// Sub Servos (SG90) - Pins 4, 5, 33, 25, 26, 27
const int SUB_SERVO_PINS[6] = {4, 5, 33, 25, 26, 27};
Servo subServos[6];

// Gate Servo (SG90) - Pin 32
const int GATE_SERVO_PIN = 32;
Servo gateServo;

// Buzzer and LED
const int BUZZER_PIN = 23;
const int LED_PIN = 19;  // Indicator LED

// Push Buttons
const int BTN_SETTINGS = 13;
const int BTN_ENTER = 14;
const int BTN_UP = 15;
const int BTN_DOWN = 18;
const int BTN_ESC = 19;

// Medication data with tablet tracking
struct Medication {
  String name;
  int tabletCount;
  int maxCapacity;
  int currentAngle;
};

Medication medications[6] = {
  {"Dolo", 0, 4, 0},
  {"Metformin", 0, 4, 0},
  {"Telma-AM", 0, 4, 0},
  {"Cetirizine", 0, 4, 0},
  {"Omega-3", 0, 4, 0},
  {"Melatonin", 0, 4, 0}
};

// System state
enum SystemMode { MANUAL_MODE, AUTOMATIC_MODE };
SystemMode systemMode = AUTOMATIC_MODE;

enum MenuState {
  HOME,
  SETTINGS_MAIN,
  SETTINGS_MODE,
  SETTINGS_FILL,
  SETTINGS_TIME,
  SETTINGS_DATE,
  SETTINGS_STATUS,
  SETTINGS_RESET,
  MANUAL_SELECT_CONTAINER,
  MANUAL_SELECT_TABLETS,
  FILL_SELECT_CONTAINER,
  FILL_OPERATION,
  SET_TIME_HOURS,
  SET_TIME_MINUTES,
  SET_TIME_AMPM,
  SET_DATE_DAY,
  SET_DATE_MONTH,
  SET_DATE_YEAR,
  EXECUTING
};

MenuState currentMenu = HOME;
int menuSelection = 0;
int selectedContainer = 0;
int tabletsToDrop = 1;

// Time/Date setting variables
int tempHours = 12;
int tempMinutes = 0;
bool tempIsPM = false;
int tempDay = 1;
int tempMonth = 1;
int tempYear = 2024;

// Button state management
int lastBtnState[5] = {HIGH, HIGH, HIGH, HIGH, HIGH};
int btnState[5] = {HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long lastDebounceTime[5] = {0, 0, 0, 0, 0};
const unsigned long debounceDelay = 50;

// Execution state
bool executing = false;
unsigned long lastExecutionTime = 0;
int executionStep = 0;
int tabletsDroppedInCurrentSequence = 0;

// Automatic scheduling
bool autoModeActive = true;
unsigned long lastAutoCheck = 0;

void setup() {
  Serial.begin(115200);

  // Initialize I2C for RTC with the pins you specified (SDA=21, SCL=22)
  Wire.begin(21, 22);

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("❌ Couldn't find RTC!");
    Serial.println("Please check RTC connections:");
    Serial.println("- VCC to 3.3V");
    Serial.println("- GND to GND"); 
    Serial.println("- SDA to GPIO 21");
    Serial.println("- SCL to GPIO 22");
    while (1);
  }

  // Set RTC to compile time if needed
  if (rtc.lostPower()) {
    Serial.println("⚠️  RTC lost power, setting to compile time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Initialize Main Servo (MG995)
  pinMode(MAIN_SERVO_PIN, OUTPUT);

  // Initialize Sub Servos (SG90)
  for (int i = 0; i < 6; i++) {
    subServos[i].attach(SUB_SERVO_PINS[i]);
    subServos[i].write(0);
    delay(100);
  }

  // Initialize Gate Servo
  gateServo.attach(GATE_SERVO_PIN);
  gateServo.write(0);

  // Initialize Buzzer and LED
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize Buttons with INPUT_PULLUP
  pinMode(BTN_SETTINGS, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ESC, INPUT_PULLUP);

  delay(1000);

  initializeServos();

  Serial.println("=== MEDI-PING: Enhanced Medication Dispenser ===");
  Serial.println("✅ System initialized successfully!");
  displayHomeScreen();
}

void loop() {
  checkButtons();

  // Check automatic schedule (runs regardless of current menu)
  if (systemMode == AUTOMATIC_MODE) {
    checkAutomaticSchedule();
  }

  if (executing) {
    executeDispensingSequence();
  }

  delay(100);
}

void checkButtons() {
  int buttons[5] = {BTN_SETTINGS, BTN_ENTER, BTN_UP, BTN_DOWN, BTN_ESC};

  for (int i = 0; i < 5; i++) {
    int reading = digitalRead(buttons[i]);

    if (reading != lastBtnState[i]) {
      lastDebounceTime[i] = millis();
    }

    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (reading != btnState[i]) {
        btnState[i] = reading;

        if (btnState[i] == LOW) {
          handleButtonPress(i);
        }
      }
    }

    lastBtnState[i] = reading;
  }
}

void handleButtonPress(int buttonIndex) {
  if (executing && currentMenu != FILL_OPERATION) return;

  switch (buttonIndex) {
    case 0: // SETTINGS
      if (currentMenu == HOME) {
        currentMenu = SETTINGS_MAIN;
        menuSelection = 0;
        displaySettingsMain();
      }
      break;

    case 1: // ENTER
      handleEnterButton();
      break;

    case 2: // UP
      handleUpButton();
      break;

    case 3: // DOWN
      handleDownButton();
      break;

    case 4: // ESC
      handleEscButton();
      break;
  }

  delay(200);
}

void handleEnterButton() {
  switch (currentMenu) {
    case HOME:
      if (systemMode == MANUAL_MODE) {
        currentMenu = MANUAL_SELECT_CONTAINER;
        menuSelection = 0;
        displayManualContainerSelection();
      }
      break;

    case SETTINGS_MAIN:
      switch (menuSelection) {
        case 0: currentMenu = SETTINGS_MODE; displayModeSelection(); break;
        case 1: currentMenu = FILL_SELECT_CONTAINER; menuSelection = 0; displayFillMenu(); break;
        case 2: currentMenu = SETTINGS_TIME; setupTimeSetting(); break;
        case 3: currentMenu = SETTINGS_DATE; setupDateSetting(); break;
        case 4: currentMenu = SETTINGS_STATUS; displayStatus(); break;
        case 5: performReset(); break;
      }
      break;

    case SETTINGS_MODE:
      systemMode = (menuSelection == 0) ? MANUAL_MODE : AUTOMATIC_MODE;
      Serial.print("✅ System mode set to: ");
      Serial.println(systemMode == MANUAL_MODE ? "MANUAL" : "AUTOMATIC");
      currentMenu = HOME;
      displayHomeScreen();
      break;

    case MANUAL_SELECT_CONTAINER:
      selectedContainer = menuSelection;
      currentMenu = MANUAL_SELECT_TABLETS;
      menuSelection = 0;
      displayManualTabletSelection();
      break;

    case MANUAL_SELECT_TABLETS:
      tabletsToDrop = menuSelection + 1;
      // Check if container has enough tablets
      if (medications[selectedContainer].tabletCount >= tabletsToDrop) {
        startDispensingSequence();
      } else {
        Serial.println("❌ Container is empty! Please fill tablets first.");
        currentMenu = MANUAL_SELECT_CONTAINER;
        displayManualContainerSelection();
      }
      break;

    case FILL_SELECT_CONTAINER:
      selectedContainer = menuSelection;
      currentMenu = FILL_OPERATION;
      displayFillOperation();
      break;

    case SET_TIME_HOURS:
      currentMenu = SET_TIME_MINUTES;
      displaySetTimeMinutes();
      break;

    case SET_TIME_MINUTES:
      currentMenu = SET_TIME_AMPM;
      displaySetTimeAMPM();
      break;

    case SET_TIME_AMPM:
      setRTCTime();
      break;

    case SET_DATE_DAY:
      currentMenu = SET_DATE_MONTH;
      displaySetDateMonth();
      break;

    case SET_DATE_MONTH:
      currentMenu = SET_DATE_YEAR;
      displaySetDateYear();
      break;

    case SET_DATE_YEAR:
      setRTCDate();
      break;

    case FILL_OPERATION:
      completeFilling();
      break;
  }
}

void handleUpButton() {
  switch (currentMenu) {
    case SETTINGS_MAIN:
    case SETTINGS_MODE:
    case MANUAL_SELECT_CONTAINER:
    case MANUAL_SELECT_TABLETS:
    case FILL_SELECT_CONTAINER:
      menuSelection--;
      if (menuSelection < 0) menuSelection = 0;
      updateDisplay();
      break;

    case SET_TIME_HOURS:
      tempHours = (tempHours % 12) + 1;
      displaySetTimeHours();
      break;

    case SET_TIME_MINUTES:
      tempMinutes = (tempMinutes + 1) % 60;
      displaySetTimeMinutes();
      break;

    case SET_TIME_AMPM:
      tempIsPM = !tempIsPM;
      displaySetTimeAMPM();
      break;

    case SET_DATE_DAY:
      tempDay = (tempDay % 31) + 1;
      displaySetDateDay();
      break;

    case SET_DATE_MONTH:
      tempMonth = (tempMonth % 12) + 1;
      displaySetDateMonth();
      break;

    case SET_DATE_YEAR:
      tempYear++;
      if (tempYear > 2030) tempYear = 2024;
      displaySetDateYear();
      break;

    case FILL_OPERATION:
      rotateFillServo(true);
      break;
  }
}

void handleDownButton() {
  int maxSelection = 0;

  switch (currentMenu) {
    case SETTINGS_MAIN:
      maxSelection = 5;
      break;
    case SETTINGS_MODE:
      maxSelection = 1;
      break;
    case MANUAL_SELECT_CONTAINER:
    case FILL_SELECT_CONTAINER:
      maxSelection = 5;
      break;
    case MANUAL_SELECT_TABLETS:
      maxSelection = 3;
      break;
  }

  switch (currentMenu) {
    case SETTINGS_MAIN:
    case SETTINGS_MODE:
    case MANUAL_SELECT_CONTAINER:
    case MANUAL_SELECT_TABLETS:
    case FILL_SELECT_CONTAINER:
      menuSelection++;
      if (menuSelection > maxSelection) menuSelection = maxSelection;
      updateDisplay();
      break;

    case SET_TIME_HOURS:
      tempHours--;
      if (tempHours < 1) tempHours = 12;
      displaySetTimeHours();
      break;

    case SET_TIME_MINUTES:
      tempMinutes--;
      if (tempMinutes < 0) tempMinutes = 59;
      displaySetTimeMinutes();
      break;

    case SET_TIME_AMPM:
      tempIsPM = !tempIsPM;
      displaySetTimeAMPM();
      break;

    case SET_DATE_DAY:
      tempDay--;
      if (tempDay < 1) tempDay = 31;
      displaySetDateDay();
      break;

    case SET_DATE_MONTH:
      tempMonth--;
      if (tempMonth < 1) tempMonth = 12;
      displaySetDateMonth();
      break;

    case SET_DATE_YEAR:
      tempYear--;
      if (tempYear < 2024) tempYear = 2030;
      displaySetDateYear();
      break;

    case FILL_OPERATION:
      rotateFillServo(false);
      break;
  }
}

void handleEscButton() {
  switch (currentMenu) {
    case SETTINGS_MAIN:
    case SETTINGS_MODE:
    case SETTINGS_STATUS:
      currentMenu = HOME;
      displayHomeScreen();
      break;

    case MANUAL_SELECT_CONTAINER:
    case MANUAL_SELECT_TABLETS:
    case FILL_SELECT_CONTAINER:
      currentMenu = HOME;
      displayHomeScreen();
      break;

    case FILL_OPERATION:
      currentMenu = FILL_SELECT_CONTAINER;
      menuSelection = 0;
      displayFillMenu();
      break;

    case SET_TIME_HOURS:
    case SET_TIME_MINUTES:
    case SET_TIME_AMPM:
    case SET_DATE_DAY:
    case SET_DATE_MONTH:
    case SET_DATE_YEAR:
      currentMenu = SETTINGS_MAIN;
      displaySettingsMain();
      break;
  }
}

// === DISPLAY FUNCTIONS ===
void displayHomeScreen() {
  DateTime now = rtc.now();

  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║           🏠 MEDI-PING HOME           ║");
  Serial.println("╠════════════════════════════════════════╣");

  // Time - proper RTC reading with verification
  Serial.print("║ Time: ");
  int displayHour = now.hour();
  String ampm = "AM";

  if (displayHour >= 12) {
    ampm = "PM";
    if (displayHour > 12) displayHour -= 12;
  }
  if (displayHour == 0) displayHour = 12;

  Serial.print(displayHour < 10 ? "0" : "");
  Serial.print(displayHour);
  Serial.print(":");
  Serial.print(now.minute() < 10 ? "0" : "");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.print(now.second() < 10 ? "0" : "");
  Serial.print(now.second());
  Serial.print(" ");
  Serial.print(ampm);
  Serial.println("               ║");

  // Date
  Serial.print("║ Date: ");
  Serial.print(now.day() < 10 ? "0" : "");
  Serial.print(now.day());
  Serial.print("/");
  Serial.print(now.month() < 10 ? "0" : "");
  Serial.print(now.month());
  Serial.print("/");
  Serial.print(now.year());
  Serial.print(" (");
  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  Serial.print(days[now.dayOfTheWeek()]);
  Serial.println(")        ║");

  // Mode
  Serial.print("║ Mode: ");
  Serial.print(systemMode == MANUAL_MODE ? "Manual" : "Automatic");
  Serial.println("                ║");

  // Recent tablet dropped
  Serial.println("║ Recent Tablets Dropped:              ║");
  bool hasRecent = false;
  for (int i = 0; i < 6; i++) {
    if (medications[i].tabletCount > 0) {
      Serial.print("║   ");
      Serial.print(medications[i].name);
      Serial.print(": ");
      Serial.print(medications[i].tabletCount);
      Serial.print(" tablets (");
      Serial.print(medications[i].currentAngle);
      Serial.println("°)           ║");
      hasRecent = true;
    }
  }
  if (!hasRecent) {
    Serial.println("║   No recent drops                  ║");
  }

  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║ SETTINGS: Enter settings menu         ║");
  if (systemMode == MANUAL_MODE) {
    Serial.println("║ ENTER: Manual dispensing             ║");
  }
  Serial.println("╚════════════════════════════════════════╝");
}

void displayManualContainerSelection() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║         SELECT CONTAINER              ║");
  Serial.println("╠════════════════════════════════════════╣");

  for (int i = 0; i < 6; i++) {
    Serial.print("║ ");
    if (i == menuSelection) Serial.print("→ ");
    else Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.print(medications[i].name);
    Serial.print(" - ");
    Serial.print(medications[i].tabletCount);
    Serial.print("/");
    Serial.print(medications[i].maxCapacity);
    Serial.print(" (");
    Serial.print(medications[i].currentAngle);
    Serial.print("°)");
    if (medications[i].tabletCount == 0) Serial.print(" (EMPTY)");
    Serial.println(" ║");
  }

  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║ ENTER: Select  ESC: Back to Home      ║");
  Serial.println("╚════════════════════════════════════════╝");
}

void displayManualTabletSelection() {
  int available = medications[selectedContainer].tabletCount;
  int currentAngle = medications[selectedContainer].currentAngle;

  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║         SELECT TABLETS TO DROP        ║");
  Serial.println("╠════════════════════════════════════════╣");

  Serial.print("║ Container: ");
  Serial.print(selectedContainer + 1);
  Serial.print(". ");
  Serial.print(medications[selectedContainer].name);
  Serial.println("         ║");

  Serial.print("║ Available tablets: ");
  Serial.print(available);
  Serial.println("           ║");

  Serial.print("║ Current angle: ");
  Serial.print(currentAngle);
  Serial.println("°                ║");

  Serial.println("║                                        ║");

  for (int i = 1; i <= 4; i++) {
    Serial.print("║ ");
    if (i == (menuSelection + 1)) Serial.print("→ ");
    else Serial.print("  ");

    Serial.print(i);
    Serial.print(" tablet");
    if (i > 1) Serial.print("s");
    Serial.print(" (");
    Serial.print(currentAngle + (i * 45));
    Serial.print("°)");

    // Show remaining tablets after this selection
    int remAfter = available - i;
    Serial.print(" - Rem: ");
    Serial.print(remAfter >= 0 ? remAfter : 0);
    Serial.println("         ║");
  }

  if (available == 0) {
    Serial.println("║                                        ║");
    Serial.println("║        ❌ CONTAINER IS EMPTY!         ║");
    Serial.println("║        Please fill tablets first       ║");
  }

  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║ ENTER: Confirm & Dispense  ESC: Back  ║");
  Serial.println("╚════════════════════════════════════════╝");
}

// === FILLING MODE FUNCTIONS ===
void displayFillMenu() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║           FILL TABLETS MENU           ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║ Select container to fill:             ║");

  for (int i = 0; i < 6; i++) {
    Serial.print("║ ");
    if (i == menuSelection) Serial.print("→ ");
    else Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.print(medications[i].name);
    Serial.print(" - ");
    Serial.print(medications[i].tabletCount);
    Serial.print("/");
    Serial.print(medications[i].maxCapacity);
    Serial.print(" (");
    Serial.print(medications[i].currentAngle);
    Serial.print("°)");
    Serial.println("       ║");
  }

  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║ ENTER: Select  ESC: Back              ║");
  Serial.println("╚════════════════════════════════════════╝");
}

void displayFillOperation() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║           FILLING OPERATION           ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.print("║ Container: ");
  Serial.print(selectedContainer + 1);
  Serial.print(". ");
  Serial.print(medications[selectedContainer].name);
  Serial.println("     ║");
  Serial.print("║ Current: ");
  Serial.print(medications[selectedContainer].tabletCount);
  Serial.print("/");
  Serial.print(medications[selectedContainer].maxCapacity);
  Serial.println(" tablets           ║");
  Serial.print("║ Angle: ");
  Serial.print(medications[selectedContainer].currentAngle);
  Serial.println("°                    ║");
  Serial.println("║                                        ║");
  Serial.println("║ UP: +45° (+1 tablet)                  ║");
  Serial.println("║ DOWN: -45° (-1 tablet)                ║");
  Serial.println("║ ENTER: Complete filling               ║");
  Serial.println("║ ESC: Back to Fill Menu                ║");
  Serial.println("╚════════════════════════════════════════╝");
}

void rotateFillServo(bool clockwise) {
  if (clockwise) {
    // Rotate clockwise (+45°) - Add tablet
    if (medications[selectedContainer].currentAngle < 180 &&
        medications[selectedContainer].tabletCount < medications[selectedContainer].maxCapacity) {
      medications[selectedContainer].currentAngle += 45;
      medications[selectedContainer].tabletCount++;
      subServos[selectedContainer].write(medications[selectedContainer].currentAngle);
      Serial.print("✅ +45° - Tablet added. Current angle: ");
      Serial.print(medications[selectedContainer].currentAngle);
      Serial.println("°");
    } else {
      Serial.println("❌ Maximum capacity reached (180° or 4 tablets)!");
    }
  } else {
    // Rotate counter-clockwise (-45°) - Remove tablet
    if (medications[selectedContainer].currentAngle > 0 &&
        medications[selectedContainer].tabletCount > 0) {
      medications[selectedContainer].currentAngle -= 45;
      medications[selectedContainer].tabletCount--;
      subServos[selectedContainer].write(medications[selectedContainer].currentAngle);
      Serial.print("✅ -45° - Tablet removed. Current angle: ");
      Serial.print(medications[selectedContainer].currentAngle);
      Serial.println("°");
    } else {
      Serial.println("❌ Already at minimum position (0° or 0 tablets)!");
    }
  }

  displayFillOperation();
}

void completeFilling() {
  Serial.println("\n✅ FILLING COMPLETED!");
  Serial.print("📦 Container ");
  Serial.print(selectedContainer + 1);
  Serial.print(" (");
  Serial.print(medications[selectedContainer].name);
  Serial.print(") now has ");
  Serial.print(medications[selectedContainer].tabletCount);
  Serial.print("/");
  Serial.print(medications[selectedContainer].maxCapacity);
  Serial.print(" tablets at ");
  Serial.print(medications[selectedContainer].currentAngle);
  Serial.println("°");

  currentMenu = HOME;
  displayHomeScreen();
}

// === SERVO CONTROL FUNCTIONS ===
void startDispensingSequence() {
  Serial.println("\n>>> STARTING DISPENSING SEQUENCE <<<");
  executing = true;
  executionStep = 0;
  tabletsDroppedInCurrentSequence = 0;
  lastExecutionTime = millis();
}

void executeDispensingSequence() {
  static unsigned long tabletStartTime = 0;
  static bool tabletInProgress = false;
  static int currentTablet = 0;

  switch (executionStep) {
    case 0: // Rotate main servo to selected container
      Serial.print("\n🔄 MAIN SERVO: Rotating to container ");
      Serial.print(selectedContainer + 1);
      Serial.print(" (");
      Serial.print(selectedContainer + 1);
      Serial.println(" rotations)");

      for (int i = 0; i < selectedContainer + 1; i++) {
        Serial.print("  Rotation ");
        Serial.print(i + 1);
        Serial.print("/");
        Serial.println(selectedContainer + 1);

        rotateServo(800, true);
        Serial.println("🛑 STOPPED");
        delay(800);
      }
      executionStep = 1;
      tabletStartTime = millis();
      currentTablet = 0;
      tabletInProgress = false;
      break;

    case 1: // Dispense tablets one by one with proper delays
      if (!tabletInProgress) {
        // Start a new tablet drop
        if (currentTablet < tabletsToDrop && medications[selectedContainer].tabletCount > 0) {
          // Calculate target angle for this tablet - MAX 180°
          int targetAngle = medications[selectedContainer].currentAngle + 45;
          
          // Ensure we don't exceed 180°
          if (targetAngle > 180) {
            targetAngle = 180;
          }
          
          Serial.print("\n📦 SUB SERVO: Rotating from ");
          Serial.print(medications[selectedContainer].currentAngle);
          Serial.print("° to ");
          Serial.print(targetAngle);
          Serial.println("°");

          // Rotate sub servo +45° for one tablet
          medications[selectedContainer].currentAngle = targetAngle;
          subServos[selectedContainer].write(medications[selectedContainer].currentAngle);
          
          tabletStartTime = millis();
          tabletInProgress = true;
          Serial.println("⏳ Moving to position...");
        } else {
          // All tablets dropped, move to next step
          executionStep = 2;
          lastExecutionTime = millis();
        }
      } else {
        // Tablet drop in progress - handle timing
        unsigned long currentTime = millis();
        
        if (currentTime - tabletStartTime > 500 && !digitalRead(LED_PIN)) {
          // After 500ms movement, operate gate
          Serial.println("🚪 GATE: Operating gate mechanism");
          digitalWrite(LED_PIN, HIGH);
          gateServo.write(180);
          beepTone(2000, 200);
          tabletStartTime = currentTime; // Reset timer for gate open time
        }
        else if (currentTime - tabletStartTime > 1800 && digitalRead(LED_PIN)) {
          // After 1800ms gate open time, close gate and complete tablet
          gateServo.write(0);
          delay(500);
          digitalWrite(LED_PIN, LOW);

          // Update medication count
          medications[selectedContainer].tabletCount--;
          tabletsDroppedInCurrentSequence++;
          currentTablet++;

          Serial.print("📊 Tablet ");
          Serial.print(currentTablet);
          Serial.print("/");
          Serial.print(tabletsToDrop);
          Serial.print(" dropped! Remaining in container: ");
          Serial.println(medications[selectedContainer].tabletCount);
          Serial.print("Current angle: ");
          Serial.print(medications[selectedContainer].currentAngle);
          Serial.println("°");

          // Check if reached 180° (max capacity) - only then return to 0°
          if (medications[selectedContainer].currentAngle >= 180) {
            Serial.println("🔄 SUB SERVO: Returning to 0° (reached 180° - all 4 tablets dropped)");
            medications[selectedContainer].currentAngle = 0;
            subServos[selectedContainer].write(0);
            delay(500);
          }

          // Wait 1 second before next tablet
          Serial.println("⏳ Waiting 1 second before next tablet...");
          tabletStartTime = millis();
        }
        else if (currentTime - tabletStartTime > 1000 && !digitalRead(LED_PIN) && tabletInProgress) {
          // After 1 second delay, move to next tablet
          tabletInProgress = false;
        }
      }
      break;

    case 2: // Return main servo to start
      Serial.println("\n🔄 MAIN SERVO: Returning to start position");
      returnToStart();
      Serial.println("✅ BACK TO START POSITION");

      Serial.println("\n✅ DISPENSING COMPLETED!");
      Serial.print("📦 Dropped ");
      Serial.print(tabletsDroppedInCurrentSequence);
      Serial.print(" tablet(s) from ");
      Serial.println(medications[selectedContainer].name);
      Serial.print("📊 Current angle maintained at: ");
      Serial.print(medications[selectedContainer].currentAngle);
      Serial.println("°");
      
      executing = false;
      currentMenu = HOME;
      displayHomeScreen();
      break;
  }
}

void rotateServo(int duration, bool clockwise) {
  unsigned long startTime = millis();
  int pulseWidth = clockwise ? 1700 : 1300;

  while (millis() - startTime < (unsigned long)duration) {
    digitalWrite(MAIN_SERVO_PIN, HIGH);
    delayMicroseconds(pulseWidth);
    digitalWrite(MAIN_SERVO_PIN, LOW);
    delay(20);
  }

  stopServo();
}

void returnToStart() {
  Serial.println("🚀 RETURNING TO START AT FULL SPEED");

  unsigned long startTime = millis();
  int returnTime = (int)(800 * (selectedContainer + 1) * 1.2);

  while (millis() - startTime < (unsigned long)returnTime) {
    digitalWrite(MAIN_SERVO_PIN, HIGH);
    delayMicroseconds(1300);
    digitalWrite(MAIN_SERVO_PIN, LOW);
    delay(20);
  }

  stopServo();
}

void stopServo() {
  digitalWrite(MAIN_SERVO_PIN, HIGH);
  delayMicroseconds(1500);
  digitalWrite(MAIN_SERVO_PIN, LOW);
  delay(20);
}

// === TIME/DATE SETTING FUNCTIONS ===
void setupTimeSetting() {
  DateTime now = rtc.now();
  tempHours = now.hour() % 12;
  if (tempHours == 0) tempHours = 12;
  tempMinutes = now.minute();
  tempIsPM = (now.hour() >= 12);
  currentMenu = SET_TIME_HOURS;
  displaySetTimeHours();
}

void displaySetTimeHours() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║             SET TIME - HOURS          ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.print("║ Current: ");
  Serial.print(tempHours);
  Serial.print(":");
  Serial.print(tempMinutes < 10 ? "0" : "");
  Serial.print(tempMinutes);
  Serial.print(tempIsPM ? " PM" : " AM");
  Serial.println("               ║");
  Serial.println("║                                        ║");
  Serial.println("║        UP: Increase hour               ║");
  Serial.println("║        DOWN: Decrease hour             ║");
  Serial.println("║        ENTER: Next (Minutes)           ║");
  Serial.println("║        ESC: Back to Settings           ║");
  Serial.println("╚════════════════════════════════════════╝");
}

void displaySetTimeMinutes() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║            SET TIME - MINUTES         ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.print("║ Current: ");
  Serial.print(tempHours);
  Serial.print(":");
  Serial.print(tempMinutes < 10 ? "0" : "");
  Serial.print(tempMinutes);
  Serial.print(tempIsPM ? " PM" : " AM");
  Serial.println("               ║");
  Serial.println("║                                        ║");
  Serial.println("║        UP: Increase minute             ║");
  Serial.println("║        DOWN: Decrease minute           ║");
  Serial.println("║        ENTER: Next (AM/PM)             ║");
  Serial.println("║        ESC: Back to Settings           ║");
  Serial.println("╚════════════════════════════════════════╝");
}

void displaySetTimeAMPM() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║             SET TIME - AM/PM          ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.print("║ Current: ");
  Serial.print(tempHours);
  Serial.print(":");
  Serial.print(tempMinutes < 10 ? "0" : "");
  Serial.print(tempMinutes);
  Serial.print(tempIsPM ? " PM" : " AM");
  Serial.println("               ║");
  Serial.println("║                                        ║");
  Serial.println("║        UP/DOWN: Toggle AM/PM           ║");
  Serial.println("║        ENTER: Set Time                 ║");
  Serial.println("║        ESC: Back to Settings           ║");
  Serial.println("╚════════════════════════════════════════╝");
}

void setRTCTime() {
  int hours24 = tempIsPM ? (tempHours == 12 ? 12 : tempHours + 12) : (tempHours == 12 ? 0 : tempHours);

  // Get current date to preserve it
  DateTime now = rtc.now();
  DateTime newTime(now.year(), now.month(), now.day(), hours24, tempMinutes, 0);

  // Actually set the RTC time with verification
  rtc.adjust(newTime);
  
  // Verify and display
  DateTime verify = rtc.now();
  Serial.printf("🔁 RTC now -> %04d/%02d/%02d %02d:%02d:%02d\n",
                verify.year(), verify.month(), verify.day(),
                verify.hour(), verify.minute(), verify.second());

  Serial.println("✅ Time set successfully!");
  currentMenu = HOME;
  displayHomeScreen();
}

void setupDateSetting() {
  DateTime now = rtc.now();
  tempDay = now.day();
  tempMonth = now.month();
  tempYear = now.year();
  currentMenu = SET_DATE_DAY;
  displaySetDateDay();
}

void displaySetDateDay() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║              SET DATE - DAY           ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.print("║ Current: ");
  Serial.print(tempDay < 10 ? "0" : "");
  Serial.print(tempDay);
  Serial.print("/");
  Serial.print(tempMonth < 10 ? "0" : "");
  Serial.print(tempMonth);
  Serial.print("/");
  Serial.print(tempYear);
  Serial.println("           ║");
  Serial.println("║                                        ║");
  Serial.println("║        UP: Increase day                ║");
  Serial.println("║        DOWN: Decrease day              ║");
  Serial.println("║        ENTER: Next (Month)             ║");
  Serial.println("║        ESC: Back to Settings           ║");
  Serial.println("╚════════════════════════════════════════╝");
}

void displaySetDateMonth() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║             SET DATE - MONTH          ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.print("║ Current: ");
  Serial.print(tempDay < 10 ? "0" : "");
  Serial.print(tempDay);
  Serial.print("/");
  Serial.print(tempMonth < 10 ? "0" : "");
  Serial.print(tempMonth);
  Serial.print("/");
  Serial.print(tempYear);
  Serial.println("           ║");
  Serial.println("║                                        ║");
  Serial.println("║        UP: Increase month              ║");
  Serial.println("║        DOWN: Decrease month            ║");
  Serial.println("║        ENTER: Next (Year)              ║");
  Serial.println("║        ESC: Back to Settings           ║");
  Serial.println("╚════════════════════════════════════════╝");
}

void displaySetDateYear() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║             SET DATE - YEAR           ║");
  Serial.println("╠════════════════════════════════════════╣");
  Serial.print("║ Current: ");
  Serial.print(tempDay < 10 ? "0" : "");
  Serial.print(tempDay);
  Serial.print("/");
  Serial.print(tempMonth < 10 ? "0" : "");
  Serial.print(tempMonth);
  Serial.print("/");
  Serial.print(tempYear);
  Serial.println("           ║");
  Serial.println("║                                        ║");
  Serial.println("║        UP: Increase year               ║");
  Serial.println("║        DOWN: Decrease year             ║");
  Serial.println("║        ENTER: Set Date                 ║");
  Serial.println("║        ESC: Back to Settings           ║");
  Serial.println("╚════════════════════════════════════════╝");
}

void setRTCDate() {
  // Get current time to preserve it
  DateTime now = rtc.now();
  DateTime newTime(tempYear, tempMonth, tempDay, now.hour(), now.minute(), now.second());

  // Actually set the RTC date with verification
  rtc.adjust(newTime);
  
  // Verify and display
  DateTime verify = rtc.now();
  Serial.printf("🔁 RTC now -> %04d/%02d/%02d %02d:%02d:%02d\n",
                verify.year(), verify.month(), verify.day(),
                verify.hour(), verify.minute(), verify.second());

  Serial.println("✅ Date set successfully!");
  currentMenu = HOME;
  displayHomeScreen();
}

// === OTHER SETTINGS FUNCTIONS ===
void displaySettingsMain() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║           ⚙️  SETTINGS MENU           ║");
  Serial.println("╠════════════════════════════════════════╣");
  String options[6] = {"1. Mode", "2. Fill Tablets", "3. Set Time", "4. Set Date", "5. Check Status", "6. Reset"};

  for (int i = 0; i < 6; i++) {
    Serial.print("║ ");
    if (i == menuSelection) Serial.print("→ ");
    else Serial.print("  ");
    Serial.print(options[i]);
    Serial.println("                         ║");
  }
  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║ ENTER: Select  ESC: Back to Home      ║");
  Serial.println("╚════════════════════════════════════════╝");
}

void displayModeSelection() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║             SELECT MODE               ║");
  Serial.println("╠════════════════════════════════════════╣");

  String modes[2] = {"1. Manual", "2. Automatic"};

  for (int i = 0; i < 2; i++) {
    Serial.print("║ ");
    if (i == menuSelection) Serial.print("→ ");
    else Serial.print("  ");
    Serial.print(modes[i]);
    Serial.println("                             ║");
  }

  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║ ENTER: Confirm  ESC: Back             ║");
  Serial.println("╚════════════════════════════════════════╝");
}

void displayStatus() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║              SYSTEM STATUS            ║");
  Serial.println("╠════════════════════════════════════════╣");

  for (int i = 0; i < 6; i++) {
    Serial.print("║ ");
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.print(medications[i].name);
    Serial.print(": ");
    Serial.print(medications[i].tabletCount);
    Serial.print("/");
    Serial.print(medications[i].maxCapacity);
    Serial.print(" tablets (");
    Serial.print(medications[i].currentAngle);
    Serial.print("°)");
    if (medications[i].tabletCount == 0) Serial.print(" - EMPTY!");
    Serial.println(" ║");
  }

  Serial.println("╠════════════════════════════════════════╣");
  Serial.println("║           ESC: Back to Settings       ║");
  Serial.println("╚════════════════════════════════════════╝");
}

void performReset() {
  Serial.println("\n🔄 Resetting system...");

  for (int i = 0; i < 6; i++) {
    medications[i].tabletCount = 0;
    medications[i].currentAngle = 0;
    subServos[i].write(0);
  }

  initializeServos();

  Serial.println("✅ System reset completed!");
  currentMenu = HOME;
  displayHomeScreen();
}

void updateDisplay() {
  switch (currentMenu) {
    case SETTINGS_MAIN:
      displaySettingsMain();
      break;
    case SETTINGS_MODE:
      displayModeSelection();
      break;
    case FILL_SELECT_CONTAINER:
      displayFillMenu();
      break;
    case MANUAL_SELECT_CONTAINER:
      displayManualContainerSelection();
      break;
    case MANUAL_SELECT_TABLETS:
      displayManualTabletSelection();
      break;
  }
}

void initializeServos() {
  Serial.println("Initializing all servos to start positions...");
  stopServo();

  for (int i = 0; i < 6; i++) {
    subServos[i].write(0);
    delay(100);
  }

  gateServo.write(0);
  delay(1000);
  Serial.println("✅ All servos initialized to start positions");
}

// === BUZZER FUNCTIONS ===
void beepTone(int freq, int duration) {
  tone(BUZZER_PIN, freq, duration);
  delay(duration);
  noTone(BUZZER_PIN);
}

// === AUTOMATIC SCHEDULER ===
void checkAutomaticSchedule() {
  // avoid checking every loop (check every 1 second)
  if (millis() - lastAutoCheck < 1000) return;
  lastAutoCheck = millis();

  DateTime now = rtc.now();
  int hour = now.hour();
  int minute = now.minute();
  int second = now.second();

  // Only check at the start of each minute (second 0-2) to avoid multiple triggers
  if (second > 2) return;

  // Morning 8:00 AM - containers 2,4,6 (indices 1,3,5)
  if (hour == 8 && minute == 0) {
    runAutoDispense({1, 3, 5}, 3); // indices for containers 2,4,6
  }
  // Afternoon 1:30 PM - containers 1,2 (indices 0,1)
  else if (hour == 13 && minute == 30) {
    runAutoDispense({0, 1}, 2); // indices for containers 1,2
  }
  // Evening 8:00 PM - containers 3,5 (indices 2,4)
  else if (hour == 20 && minute == 0) {
    runAutoDispense({2, 4}, 2); // indices for containers 3,5
  }
}

void runAutoDispense(std::initializer_list<int> comps, int count) {
  if (executing) return; // Don't start if already executing

  digitalWrite(LED_PIN, HIGH);
  Serial.println("💊 AUTO DISPENSING STARTED...");
  beepTone(2000, 200);

  for (int comp : comps) {
    Serial.printf("Dropping tablet from compartment %d (%s)\n", comp + 1, medications[comp].name);
    
    // Check if container has tablets
    if (medications[comp].tabletCount > 0) {
      selectedContainer = comp;
      tabletsToDrop = 1;
      startDispensingSequence();
      
      // Wait for current dispensing to complete
      while (executing) {
        executeDispensingSequence();
        delay(100);
      }
      
      delay(2000); // Delay between compartments
    } else {
      Serial.printf("❌ Container %d (%s) is empty! Skipping.\n", comp + 1, medications[comp].name);
    }
  }

  digitalWrite(LED_PIN, LOW);
  Serial.println("✅ AUTO DISPENSING COMPLETED\n");
}