/*
 * ============================================================================
 * TRI-CONTROL HUB - 3-in-1 Smart RC Car
 * File: code/joystick_controller.ino
 * Board: Arduino UNO
 *
 * REAL CAR STYLE CONTROL CONCEPT:
 * - 10K Potentiometer (A2) = STEERING WHEEL (Left / Straight / Right)
 * - Analog Joystick Y (A1) = ACCELERATOR ONLY (Forward / Stop / Backward)
 * - Analog Joystick X (A0) = NOT USED (Ignored in code)
 * - Bell Push Switch (D2)  = Horn (RC Mode) + Jump (Dino Mode)
 * - 3 Ohm Mini Speaker (D6 via 330 Ohm Resistor) = ALL SOUNDS (No transistor!)
 *
 * EXACT ARDUINO UNO PIN MAP:
 * - A0  -> Joystick VRx (NOT USED)
 * - A1  -> Joystick VRy (Accelerator: Forward / Backward)
 * - A2  -> 10K Potentiometer Wiper (Steering: 0-400 Left, 401-620 Straight, 621-1023 Right)
 * - A4  -> LCD AIP31068 SDA (I2C Data)
 * - A5  -> LCD AIP31068 SCL (I2C Clock)
 * - D2  -> Bell Push Switch (Active LOW, INPUT_PULLUP, leg to GND)
 * - D3  -> Ultrasonic HC-SR04 Trig
 * - D4  -> Ultrasonic HC-SR04 Echo
 * - D5  -> SG90 Servo Signal (0-180 deg)
 * - D6  -> 3 Ohm Mini Speaker (+) via 330 Ohm Resistor -> GND
 * - D7  -> Button1 (Headlight Toggle, Active LOW, INPUT_PULLUP)
 * - D8  -> Button2 (Backlight Toggle, Active LOW, INPUT_PULLUP)
 * - D9  -> Master Button (Long Press 2 Sec = Mode Change, INPUT_PULLUP)
 * - D10 -> HC-05 TX (Arduino SoftwareSerial RX)
 * - D11 -> HC-05 RX (Arduino SoftwareSerial TX)
 * ============================================================================
 */

#include <Wire.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define PIN_JOY_Y        A1   // Accelerator ONLY
#define PIN_STEER_POT    A2   // 10K B10K Potentiometer Wiper (Steering)

#define PIN_BELL_SW      2    // Doorbell Push Switch (Horn + Dino Jump)
#define PIN_TRIG         3    // HC-SR04 Trig
#define PIN_ECHO         4    // HC-SR04 Echo
#define PIN_SERVO        5    // SG90 Servo Signal
#define PIN_SPEAKER      6    // 3 Ohm Speaker via 330 Ohm Resistor (NO transistor)
#define PIN_BTN_HEAD     7    // Button 1 - Headlight Toggle
#define PIN_BTN_BACK     8    // Button 2 - Backlight Toggle
#define PIN_BTN_MASTER   9    // Master Button (2s Long Press = Mode Change)
#define PIN_BT_RX        10   // Connects to HC-05 TX
#define PIN_BT_TX        11   // Connects to HC-05 RX

// ============================================================================
// CALIBRATION THRESHOLDS
// ============================================================================
// Potentiometer Steering Thresholds
const int POT_LEFT_MAX     = 400;  // 0 - 400 = LEFT
const int POT_STRAIGHT_MAX = 620;  // 401 - 620 = STRAIGHT (Deadzone), 621-1023 = RIGHT

// Joystick Y-Axis Accelerator Thresholds
// If pushing UP reads > 650 on your module, keep true; if pushing UP reads < 350, set false
#define JOY_UP_HIGH_VALUE true
const int JOY_HIGH_THRESH = 650;
const int JOY_LOW_THRESH  = 350;

// ============================================================================
// BUILT-IN LIGHTWEIGHT I2C LCD DRIVER FOR AIP31068 (0x3E)
// (Zero external LCD library required! Uses built-in Wire.h)
// ============================================================================
class AIP31068_LCD {
  private:
    uint8_t _addr;

    void sendByte(uint8_t control, uint8_t value) {
      Wire.beginTransmission(_addr);
      Wire.write(control);
      Wire.write(value);
      Wire.endTransmission();
    }

  public:
    AIP31068_LCD(uint8_t addr = 0x3E) : _addr(addr) {}

    void begin() {
      Wire.begin();
      delay(50);
      command(0x38); // 8-bit, 2-line, 5x8 dots
      delayMicroseconds(50);
      command(0x0C); // Display ON, Cursor OFF, Blink OFF
      delayMicroseconds(50);
      clear();
      command(0x06); // Entry mode: increment cursor
      delayMicroseconds(50);
    }

    void command(uint8_t cmd) {
      sendByte(0x80, cmd);
    }

    void writeChar(uint8_t data) {
      sendByte(0x40, data);
    }

    void clear() {
      command(0x01);
      delay(2);
    }

    void setCursor(uint8_t col, uint8_t row) {
      uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
      command(addr);
    }

    void createChar(uint8_t location, const uint8_t charmap[]) {
      location &= 0x7;
      command(0x40 | (location << 3));
      for (int i = 0; i < 8; i++) {
        writeChar(charmap[i]);
      }
      setCursor(0, 0);
    }

    void print(const char* str) {
      while (*str) {
        writeChar((uint8_t)(*str++));
      }
    }

    void print(String s) {
      print(s.c_str());
    }

    void print(int val) {
      char buf[12];
      itoa(val, buf, 10);
      print(buf);
    }

    // Print exactly 16 characters on a row to avoid flicker from clear()
    void printRow(uint8_t row, String text) {
      setCursor(0, row);
      int len = text.length();
      for (int i = 0; i < 16; i++) {
        if (i < len) {
          writeChar((uint8_t)text[i]);
        } else {
          writeChar(' ');
        }
      }
    }
};

// ============================================================================
// SYSTEM OBJECTS & STATE
// ============================================================================
AIP31068_LCD lcd(0x3E);
Servo radarServo;
SoftwareSerial btSerial(PIN_BT_RX, PIN_BT_TX); // RX=D10, TX=D11

enum SystemMode {
  MODE_RC_CAR = 0,
  MODE_RADAR = 1,
  MODE_DINO = 2
};

SystemMode currentMode = MODE_RC_CAR;

// Master Button Long-Press Tracking (2000ms)
bool masterBtnPressed = false;
unsigned long masterBtnDownMs = 0;
bool masterSwitchTriggered = false;
const unsigned long LONG_PRESS_MS = 2000;

// Debounced Button States (RC Mode)
bool lastBellState = HIGH;
bool lastHeadState = HIGH;
bool lastBackState = HIGH;
unsigned long lastBellDebounceMs = 0;
unsigned long lastHeadDebounceMs = 0;
unsigned long lastBackDebounceMs = 0;
const unsigned long DEBOUNCE_MS = 60;

// RC Car Command Tracking
String lastSentCmd = "";
unsigned long lastCmdSendMs = 0;
unsigned long lastRcLcdMs = 0;

// Bluetooth Connection Alert Tracking (Function 5)
bool btConnected = false;
bool everReceivedBtData = false;
unsigned long lastBtRxMs = 0;
const unsigned long BT_DISCONNECT_TIMEOUT_MS = 3000;

// Radar Mode State (Function 2)
int radarAngle = 0;
int radarStep = 2;
int lastDistanceCm = 99;
unsigned long lastServoMoveMs = 0;
unsigned long lastSonarBeepMs = 0;
unsigned long lastRadarLcdMs = 0;
unsigned long lastRadarBtSendMs = 0;

// Dino Game Mode State (Function 3)
bool dinoGameOver = false;
bool dinoJumping = false;
unsigned long dinoJumpStartMs = 0;
const unsigned long DINO_JUMP_DURATION_MS = 600;
int cactusPos = 15;
int dinoScore = 0;
unsigned long lastDinoTickMs = 0;
unsigned long dinoTickSpeedMs = 220;
bool cactusClearedScored = false;

// Custom LCD Icons for Dino Game
const uint8_t CHAR_DINO[8] = {
  0b00111,
  0b00101,
  0b00111,
  0b10110,
  0b11111,
  0b01110,
  0b01010,
  0b11011
};

const uint8_t CHAR_CACTUS[8] = {
  0b00100,
  0b10100,
  0b10101,
  0b11101,
  0b00111,
  0b00100,
  0b00100,
  0b00100
};

// ============================================================================
// SPEAKER MELODY FUNCTIONS (PIN D6 via 330 OHM RESISTOR - NO TRANSISTOR)
// ============================================================================

// Function 5: Startup Melody (C-E-G-C)
void playStartupMelody() {
  tone(PIN_SPEAKER, 523, 150); delay(170);
  tone(PIN_SPEAKER, 659, 150); delay(170);
  tone(PIN_SPEAKER, 784, 150); delay(170);
  tone(PIN_SPEAKER, 1046, 300); delay(320);
  noTone(PIN_SPEAKER);
}

// Function 5: Bluetooth Connect Happy Melody
void playBtConnectMelody() {
  tone(PIN_SPEAKER, 523, 150); delay(170);
  tone(PIN_SPEAKER, 659, 150); delay(170);
  tone(PIN_SPEAKER, 784, 300); delay(320);
  noTone(PIN_SPEAKER);
}

// Function 5: Bluetooth Disconnect Sad Melody
void playBtDisconnectMelody() {
  tone(PIN_SPEAKER, 784, 150); delay(170);
  tone(PIN_SPEAKER, 659, 150); delay(170);
  tone(PIN_SPEAKER, 523, 300); delay(320);
  noTone(PIN_SPEAKER);
}

// Function 5: Mode Change Melody (2 short beeps)
void playModeChangeMelody() {
  tone(PIN_SPEAKER, 1000, 100); delay(140);
  tone(PIN_SPEAKER, 1000, 100); delay(140);
  noTone(PIN_SPEAKER);
}

// Function 3: Dino Game Start Ascending Melody
void playDinoStartMelody() {
  tone(PIN_SPEAKER, 523, 150); delay(170);
  tone(PIN_SPEAKER, 659, 150); delay(170);
  tone(PIN_SPEAKER, 784, 300); delay(320);
  noTone(PIN_SPEAKER);
}

// Function 3: Dino Game Over Descending Sad Melody
void playDinoGameOverMelody() {
  tone(PIN_SPEAKER, 400, 200); delay(220);
  tone(PIN_SPEAKER, 300, 200); delay(220);
  tone(PIN_SPEAKER, 200, 200); delay(220);
  tone(PIN_SPEAKER, 100, 400); delay(420);
  noTone(PIN_SPEAKER);
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);

  // Input Pins with internal pull-ups (Active LOW buttons to GND)
  pinMode(PIN_BELL_SW, INPUT_PULLUP);
  pinMode(PIN_BTN_HEAD, INPUT_PULLUP);
  pinMode(PIN_BTN_BACK, INPUT_PULLUP);
  pinMode(PIN_BTN_MASTER, INPUT_PULLUP);

  // Ultrasonic & Speaker Pins
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_SPEAKER, OUTPUT);

  // Attach Radar Servo and center at 90 degrees
  radarServo.attach(PIN_SERVO);
  radarServo.write(90);

  // Initialize AIP31068 16x2 I2C LCD
  lcd.begin();
  lcd.createChar(0, CHAR_DINO);
  lcd.createChar(1, CHAR_CACTUS);

  lcd.printRow(0, "TRI-CONTROL HUB");
  lcd.printRow(1, "3-in-1 Smart Car");

  // Function 5: Startup Melody
  playStartupMelody();

  lcd.clear();
  showCurrentModeSplash();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  // 1. Function 5: Monitor Bluetooth incoming 'C' and 3-second disconnect watchdog
  monitorBluetoothConnection();

  // 2. Check Master Button (D9) for 2-second Long Press mode cycle
  checkMasterButton();

  // 3. Run active mode handler
  switch (currentMode) {
    case MODE_RC_CAR:
      runRcCarMode();
      break;
    case MODE_RADAR:
      runRadarMode();
      break;
    case MODE_DINO:
      runDinoGameMode();
      break;
  }
}

// ============================================================================
// FUNCTION 5: BLUETOOTH CONNECT / DISCONNECT ALERT
// ============================================================================
void monitorBluetoothConnection() {
  while (btSerial.available() > 0) {
    char c = (char)btSerial.read();
    if (c == 'C') {
      lastBtRxMs = millis();
      everReceivedBtData = true;
      if (!btConnected) {
        btConnected = true;
        playBtConnectMelody();
      }
    }
  }

  // If no data received for 3 seconds while connected, play Disconnect Sad Melody
  if (btConnected && everReceivedBtData && (millis() - lastBtRxMs >= BT_DISCONNECT_TIMEOUT_MS)) {
    btConnected = false;
    playBtDisconnectMelody();
  }
}

// ============================================================================
// MASTER BUTTON (D9): LONG PRESS 2 SECONDS TO CHANGE MODE
// ============================================================================
void checkMasterButton() {
  bool reading = digitalRead(PIN_BTN_MASTER);

  if (reading == LOW) {
    if (!masterBtnPressed) {
      masterBtnPressed = true;
      masterBtnDownMs = millis();
      masterSwitchTriggered = false;
    } else if (!masterSwitchTriggered && (millis() - masterBtnDownMs >= LONG_PRESS_MS)) {
      masterSwitchTriggered = true;
      cycleNextMode();
    }
  } else {
    masterBtnPressed = false;
    masterSwitchTriggered = false;
  }
}

void cycleNextMode() {
  // Always stop motors safely when leaving RC mode
  if (currentMode == MODE_RC_CAR) {
    btSerial.println("S");
    lastSentCmd = "S";
  }

  currentMode = (SystemMode)((currentMode + 1) % 3);
  playModeChangeMelody();
  lcd.clear();
  showCurrentModeSplash();

  if (currentMode == MODE_RADAR) {
    radarAngle = 0;
    radarStep = 2;
    radarServo.write(0);
    lastServoMoveMs = millis();
    lastSonarBeepMs = millis();
  } else if (currentMode == MODE_DINO) {
    initDinoGame();
  }
}

void showCurrentModeSplash() {
  if (currentMode == MODE_RC_CAR) {
    lcd.printRow(0, "MODE: RC CAR");
    lcd.printRow(1, "POT:STR JOY:ACC");
    delay(600);
  } else if (currentMode == MODE_RADAR) {
    lcd.printRow(0, "MODE: RADAR");
    lcd.printRow(1, "SCANNING 0-180");
    delay(600);
  } else {
    lcd.printRow(0, "MODE: DINO GAME");
    lcd.printRow(1, "BELL SW = JUMP");
    delay(600);
  }
}

// ============================================================================
// MODE 1: RC CAR MODE (POTENTIOMETER STEERING + JOYSTICK Y ACCELERATOR)
// ============================================================================
void runRcCarMode() {
  unsigned long now = millis();

  // Read Potentiometer (A2) for Steering
  int potVal = analogRead(PIN_STEER_POT);
  String steerLabel;
  if (potVal <= POT_LEFT_MAX) {
    steerLabel = "LEFT";
  } else if (potVal <= POT_STRAIGHT_MAX) {
    steerLabel = "STR";
  } else {
    steerLabel = "RIGHT";
  }

  // Read Joystick Y (A1) ONLY for Accelerator (Joystick X A0 is ignored)
  int joyY = analogRead(PIN_JOY_Y);
  String accelLabel;
  if (JOY_UP_HIGH_VALUE) {
    if (joyY >= JOY_HIGH_THRESH) accelLabel = "FWD";
    else if (joyY <= JOY_LOW_THRESH) accelLabel = "BACK";
    else accelLabel = "STOP";
  } else {
    if (joyY <= JOY_LOW_THRESH) accelLabel = "FWD";
    else if (joyY >= JOY_HIGH_THRESH) accelLabel = "BACK";
    else accelLabel = "STOP";
  }

  // Combined Steering + Accelerator Command Logic
  String cmd = "S";
  if (accelLabel == "FWD") {
    if (steerLabel == "LEFT") cmd = "FL";
    else if (steerLabel == "RIGHT") cmd = "FR";
    else cmd = "F";
  } else if (accelLabel == "BACK") {
    if (steerLabel == "LEFT") cmd = "BL";
    else if (steerLabel == "RIGHT") cmd = "BR";
    else cmd = "B";
  } else {
    // Accelerator in Middle
    if (steerLabel == "LEFT") cmd = "L";
    else if (steerLabel == "RIGHT") cmd = "R";
    else cmd = "S";
  }

  // Transmit command when changed or periodic refresh every 150ms
  if (cmd != lastSentCmd || (now - lastCmdSendMs >= 150)) {
    btSerial.println(cmd);
    lastSentCmd = cmd;
    lastCmdSendMs = now;
  }

  // Button 1 (D7): Headlight Toggle ('H')
  bool headReading = digitalRead(PIN_BTN_HEAD);
  if (headReading == LOW && lastHeadState == HIGH && (now - lastHeadDebounceMs > DEBOUNCE_MS)) {
    btSerial.println("H");
    lastHeadDebounceMs = now;
  }
  lastHeadState = headReading;

  // Button 2 (D8): Backlight Toggle ('T')
  bool backReading = digitalRead(PIN_BTN_BACK);
  if (backReading == LOW && lastBackState == HIGH && (now - lastBackDebounceMs > DEBOUNCE_MS)) {
    btSerial.println("T");
    lastBackDebounceMs = now;
  }
  lastBackState = backReading;

  // Bell Push Switch (D2): Horn ('O' + tone(D6, 800, 200))
  bool bellReading = digitalRead(PIN_BELL_SW);
  if (bellReading == LOW && lastBellState == HIGH && (now - lastBellDebounceMs > DEBOUNCE_MS)) {
    btSerial.println("O");
    tone(PIN_SPEAKER, 800, 200);
    lastBellDebounceMs = now;
  }
  lastBellState = bellReading;

  // Update LCD every 150ms without flicker
  if (now - lastRcLcdMs >= 150) {
    lastRcLcdMs = now;
    String row1 = "MODE: RC CAR";
    if (btConnected) row1 += " BT";
    String row2 = steerLabel + " " + accelLabel + " [" + cmd + "]";
    lcd.printRow(0, row1);
    lcd.printRow(1, row2);
  }
}

// ============================================================================
// MODE 2: RADAR MODE (SERVO 0-180 + ULTRASONIC + FUNCTION 2 SONAR BEEP)
// ============================================================================
int measureDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // 20ms timeout (~340cm max) so servo and buttons stay responsive
  long duration = pulseIn(PIN_ECHO, HIGH, 20000);
  if (duration == 0) {
    return 100; // Out of range -> Safe
  }
  int cm = (int)(duration * 0.034 / 2.0);
  return constrain(cm, 1, 200);
}

void runRadarMode() {
  unsigned long now = millis();

  // Sweep Servo 0 - 180 degrees smoothly using millis()
  if (now - lastServoMoveMs >= 30) {
    lastServoMoveMs = now;
    radarServo.write(radarAngle);
    lastDistanceCm = measureDistanceCm();

    radarAngle += radarStep;
    if (radarAngle >= 180) {
      radarAngle = 180;
      radarStep = -2;
    } else if (radarAngle <= 0) {
      radarAngle = 0;
      radarStep = 2;
    }
  }

  // Determine Sonar Beep Interval & LCD Alert Label (Function 2)
  unsigned long beepIntervalMs = 1000;
  String zoneLabel = "SAFE";

  if (lastDistanceCm < 10) {
    beepIntervalMs = 100;
    zoneLabel = "STOP!!";
  } else if (lastDistanceCm <= 15) {
    beepIntervalMs = 200;
    zoneLabel = "DANGER!";
  } else if (lastDistanceCm <= 30) {
    beepIntervalMs = 500;
    zoneLabel = "WARNING";
  } else {
    beepIntervalMs = 1000;
    zoneLabel = "SAFE";
  }

  // Non-blocking Radar Sonar Beep using millis()
  if (now - lastSonarBeepMs >= beepIntervalMs) {
    lastSonarBeepMs = now;
    tone(PIN_SPEAKER, 1000, 100);
  }

  // Transmit Distance + Angle via Bluetooth every 150ms
  if (now - lastRadarBtSendMs >= 150) {
    lastRadarBtSendMs = now;
    btSerial.print("D:");
    btSerial.print(lastDistanceCm);
    btSerial.print(",A:");
    btSerial.println(radarAngle);
  }

  // Update LCD every 120ms
  if (now - lastRadarLcdMs >= 120) {
    lastRadarLcdMs = now;
    String row1 = "RADAR A:" + String(radarAngle) + "deg";
    String row2 = String(lastDistanceCm) + "cm " + zoneLabel;
    lcd.printRow(0, row1);
    lcd.printRow(1, row2);
  }
}

// ============================================================================
// MODE 3: DINO GAME MODE (LCD + BELL SWITCH JUMP + FUNCTION 3 SOUNDS)
// ============================================================================
void initDinoGame() {
  dinoGameOver = false;
  dinoJumping = false;
  cactusPos = 15;
  dinoScore = 0;
  dinoTickSpeedMs = 220;
  cactusClearedScored = false;
  lastDinoTickMs = millis();

  lcd.clear();
  playDinoStartMelody();
}

bool isJoystickUpPressed() {
  int joyY = analogRead(PIN_JOY_Y);
  if (JOY_UP_HIGH_VALUE) return (joyY >= JOY_HIGH_THRESH);
  return (joyY <= JOY_LOW_THRESH);
}

void runDinoGameMode() {
  unsigned long now = millis();

  // Check Bell Switch (D2) OR Joystick UP (A1)
  bool bellPressed = (digitalRead(PIN_BELL_SW) == LOW);
  bool joyUp = isJoystickUpPressed();
  bool jumpInput = bellPressed || joyUp;

  // Handle Game Over state -> Press Bell Switch or Joystick UP to restart
  if (dinoGameOver) {
    if (bellPressed && (now - lastBellDebounceMs > 300)) {
      lastBellDebounceMs = now;
      initDinoGame();
    }
    return;
  }

  // Trigger Jump (Function 3: Boing sound)
  if (jumpInput && !dinoJumping) {
    dinoJumping = true;
    dinoJumpStartMs = now;
    tone(PIN_SPEAKER, 600, 150);
  }

  // End Jump after air-time duration
  if (dinoJumping && (now - dinoJumpStartMs >= DINO_JUMP_DURATION_MS)) {
    dinoJumping = false;
  }

  // Game Tick (non-blocking)
  if (now - lastDinoTickMs >= dinoTickSpeedMs) {
    lastDinoTickMs = now;

    // Move Cactus left
    cactusPos--;
    if (cactusPos < 0) {
      cactusPos = 15;
      cactusClearedScored = false;
      // Slightly increase speed as score grows
      if (dinoTickSpeedMs > 110) {
        dinoTickSpeedMs -= 5;
      }
    }

    // Score +10 when Dino successfully jumps over Cactus (Function 3: Ting sound)
    if (cactusPos == 1 && dinoJumping && !cactusClearedScored) {
      dinoScore += 10;
      cactusClearedScored = true;
      tone(PIN_SPEAKER, 1200, 50);
    }

    // Collision Check at column 2 (Dino column) on ground (not jumping)
    if (cactusPos == 2 && !dinoJumping) {
      dinoGameOver = true;
      lcd.printRow(0, "GAME OVER!");
      lcd.printRow(1, "SCORE: " + String(dinoScore));
      playDinoGameOverMelody();
      return;
    }

    // Render Dino & Cactus Frame on 16x2 LCD
    drawDinoFrame();
  }
}

void drawDinoFrame() {
  // Row 0: Jumping Dino (at col 2) + Score on right
  lcd.printRow(0, "        SC:" + String(dinoScore));
  if (dinoJumping) {
    lcd.setCursor(2, 0);
    lcd.writeChar(0); // Custom Dino character on top row
  }

  // Row 1: Ground line with Dino (if not jumping) + Cactus
  lcd.setCursor(0, 1);
  for (int col = 0; col < 16; col++) {
    if (col == 2 && !dinoJumping) {
      lcd.writeChar(0); // Dino
    } else if (col == cactusPos) {
      lcd.writeChar(1); // Cactus
    } else {
      lcd.writeChar('_'); // Ground
    }
  }
}
