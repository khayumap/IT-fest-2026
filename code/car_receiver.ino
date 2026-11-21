/*
 * ============================================================================
 * TRI-CONTROL HUB - 3-in-1 Smart RC Car
 * File: code/car_receiver.ino
 * Board: NodeMCU ESP8266 (ESP-12E)
 *
 * CAR HARDWARE:
 * - NodeMCU ESP8266
 * - L298N Motor Driver
 * - 2 DC Motors in FRONT (Left + Right Drive Wheels)
 * - 1 Caster Wheel in BACK (Support)
 * - 7.4V Lithium Battery (2x 18650) -> L298N 12V Input
 * - HC-05 Bluetooth Slave Module
 * - NO Speaker / NO Buzzer in Car (Car is completely silent)
 *
 * EXACT PIN MAP (DO NOT CHANGE):
 * - D2 (GPIO4)  -> Headlight (+) via 220 Ohm resistor, LED to GND
 * - D3 (GPIO0)  -> Backlight (+) via 220 Ohm resistor, LED to GND
 * - D5 (GPIO14) -> L298N IN1 (Left Motor Forward PWM)
 * - D6 (GPIO12) -> L298N IN2 (Left Motor Reverse PWM)
 * - D7 (GPIO13) -> L298N IN3 (Right Motor Forward PWM)
 * - D8 (GPIO15) -> L298N IN4 (Right Motor Reverse PWM)
 * - D0 (GPIO16) -> Status LED Green (+) - Bluetooth Connected Indicator
 * - D1 (GPIO5)  -> Status LED Red (+)   - Bluetooth Disconnected Indicator
 * - A0          -> HC-05 STATE Pin (HIGH = Connected, LOW = Disconnected)
 * - RX          -> HC-05 TX Pin
 * - TX          -> HC-05 RX Pin (via voltage divider 1k + 2k Ohm)
 * - VIN         -> L298N 5V Output
 * - GND         -> Common Ground Bus
 *
 * NOTE: Unplug HC-05 RX/TX from NodeMCU RX/TX while uploading this sketch!
 * ============================================================================
 */

// Exact NodeMCU ESP8266 Pin Definitions
#define PIN_LED_GREEN  D0  // GPIO16 - Bluetooth Connected LED
#define PIN_LED_RED    D1  // GPIO5  - Bluetooth Disconnected LED
#define PIN_HEADLIGHT  D2  // GPIO4  - Front Headlight LED
#define PIN_BACKLIGHT  D3  // GPIO0  - Rear Backlight LED

#define PIN_IN1        D5  // GPIO14 - Left Motor Forward (PWM)
#define PIN_IN2        D6  // GPIO12 - Left Motor Reverse (PWM)
#define PIN_IN3        D7  // GPIO13 - Right Motor Forward (PWM)
#define PIN_IN4        D8  // GPIO15 - Right Motor Reverse (PWM)

#define PIN_BT_STATE   A0  // Analog pin reading HC-05 STATE pin

// PWM Speed Constants (0 - 255)
const int SPEED_FULL = 255;
const int SPEED_HALF = 128;
const int SPEED_OFF  = 0;

// Light States
bool headlightOn = false;
bool backlightOn = false;

// Non-blocking Horn Headlight Blink State (NO sound in Car)
bool hornBlinkActive = false;
int hornBlinkStep = 0;
unsigned long hornBlinkTimer = 0;

// Bluetooth Telemetry & Heartbeat State
bool lastBtConnected = false;
unsigned long lastHeartbeatMs = 0;
const unsigned long HEARTBEAT_INTERVAL_MS = 1500;

// Serial Command Buffer
String cmdBuffer = "";
unsigned long lastSerialCharMs = 0;
const unsigned long CMD_FLUSH_TIMEOUT_MS = 35;

void setup() {
  // Hardware Serial for HC-05 Bluetooth Module (9600 baud default)
  Serial.begin(9600);

  // Ensure consistent 8-bit PWM range (0-255) across ESP8266 Arduino core versions
  analogWriteRange(255);

  // Configure Status & Light Pins
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_HEADLIGHT, OUTPUT);
  pinMode(PIN_BACKLIGHT, OUTPUT);

  // Configure Motor Driver Pins
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);

  // Initialize safe startup states
  stopMotors();
  digitalWrite(PIN_HEADLIGHT, LOW);
  digitalWrite(PIN_BACKLIGHT, LOW);

  // Read initial HC-05 STATE on A0
  updateBluetoothStatusLED();

  // Send 'C' on startup to confirm Car is ready / connected to Joystick
  delay(200);
  Serial.println("C");
  lastHeartbeatMs = millis();
}

void loop() {
  // 1. Continuously monitor HC-05 STATE pin on A0
  updateBluetoothStatusLED();

  // 2. Read incoming Bluetooth commands on Hardware Serial RX
  readBluetoothCommands();

  // 3. Handle non-blocking Headlight double-blink for Horn ('O')
  handleHornHeadlightBlink();

  // 4. Send periodic 'C' heartbeat while HC-05 STATE is HIGH so Joystick
  //    knows link is alive (prevents false 3-second disconnect alarm)
  if (lastBtConnected && (millis() - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS)) {
    Serial.println("C");
    lastHeartbeatMs = millis();
  }
}

/*
 * Reads A0 (HC-05 STATE pin).
 * ESP8266 A0 range is 0-1023 (0V - 3.3V). HIGH (~3.3V) reads > 512.
 * If HIGH -> D0 Green ON, D1 Red OFF
 * If LOW  -> D0 Green OFF, D1 Red ON
 */
void updateBluetoothStatusLED() {
  int stateRaw = analogRead(PIN_BT_STATE);
  bool isConnected = (stateRaw > 512);

  if (isConnected) {
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_RED, LOW);
  } else {
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
  }

  // On transition from Disconnected -> Connected, send 'C' to trigger Joystick Happy Melody
  if (isConnected && !lastBtConnected) {
    Serial.println("C");
    lastHeartbeatMs = millis();
  }
  lastBtConnected = isConnected;
}

/*
 * Reads Serial commands from HC-05.
 * Supports newline-delimited commands ("FL\n", "F\n", "S\n", etc.)
 * as well as raw single/two-character bursts with a short flush timeout.
 */
void readBluetoothCommands() {
  while (Serial.available() > 0) {
    char ch = (char)Serial.read();
    lastSerialCharMs = millis();

    if (ch == '\n' || ch == '\r') {
      if (cmdBuffer.length() > 0) {
        processCommand(cmdBuffer);
        cmdBuffer = "";
      }
    } else if (ch >= ' ' && ch <= '~') {
      cmdBuffer += ch;
      // Guard against runaway buffer
      if (cmdBuffer.length() > 24) {
        cmdBuffer = "";
      }
    }
  }

  // If a sender (e.g. phone app without newline) sent 1-2 chars without '\n', flush after 35ms
  if (cmdBuffer.length() > 0 && (millis() - lastSerialCharMs >= CMD_FLUSH_TIMEOUT_MS)) {
    processCommand(cmdBuffer);
    cmdBuffer = "";
  }
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;

  // 8-Direction Front-Wheel Differential Drive + Stop
  if (cmd == "F") {
    moveForward();
  } else if (cmd == "B") {
    moveBackward();
  } else if (cmd == "L") {
    turnLeft();
  } else if (cmd == "R") {
    turnRight();
  } else if (cmd == "FL") {
    moveForwardLeft();
  } else if (cmd == "FR") {
    moveForwardRight();
  } else if (cmd == "BL") {
    moveBackwardLeft();
  } else if (cmd == "BR") {
    moveBackwardRight();
  } else if (cmd == "S") {
    stopMotors();
  } else if (cmd == "H") {
    toggleHeadlight();
  } else if (cmd == "T") {
    toggleBacklight();
  } else if (cmd == "O") {
    triggerHornBlink();
  }
}

// ============================================================================
// MOTOR CONTROL FUNCTIONS - Front-Wheel Drive with Rear Caster
// D5 (IN1): Left Forward  | D6 (IN2): Left Reverse
// D7 (IN3): Right Forward | D8 (IN4): Right Reverse
// ============================================================================

void setMotors(int in1, int in2, int in3, int in4) {
  analogWrite(PIN_IN1, in1);
  analogWrite(PIN_IN2, in2);
  analogWrite(PIN_IN3, in3);
  analogWrite(PIN_IN4, in4);
}

// Forward: D5 HIGH + D7 HIGH, D6 LOW, D8 LOW
void moveForward() {
  setMotors(SPEED_FULL, SPEED_OFF, SPEED_FULL, SPEED_OFF);
}

// Backward: D6 HIGH + D8 HIGH, D5 LOW, D7 LOW
void moveBackward() {
  setMotors(SPEED_OFF, SPEED_FULL, SPEED_OFF, SPEED_FULL);
}

// Left (Pivot): D7 HIGH + D5 LOW, D6 LOW, D8 LOW
void turnLeft() {
  setMotors(SPEED_OFF, SPEED_OFF, SPEED_FULL, SPEED_OFF);
}

// Right (Pivot): D5 HIGH + D7 LOW, D6 LOW, D8 LOW
void turnRight() {
  setMotors(SPEED_FULL, SPEED_OFF, SPEED_OFF, SPEED_OFF);
}

// Forward-Left: D7 Full Speed + D5 Half Speed
void moveForwardLeft() {
  setMotors(SPEED_HALF, SPEED_OFF, SPEED_FULL, SPEED_OFF);
}

// Forward-Right: D5 Full Speed + D7 Half Speed
void moveForwardRight() {
  setMotors(SPEED_FULL, SPEED_OFF, SPEED_HALF, SPEED_OFF);
}

// Backward-Left: D8 Full Speed + D6 Half Speed
void moveBackwardLeft() {
  setMotors(SPEED_OFF, SPEED_HALF, SPEED_OFF, SPEED_FULL);
}

// Backward-Right: D6 Full Speed + D8 Half Speed
void moveBackwardRight() {
  setMotors(SPEED_OFF, SPEED_FULL, SPEED_OFF, SPEED_HALF);
}

// Stop: D5 LOW, D6 LOW, D7 LOW, D8 LOW
void stopMotors() {
  setMotors(SPEED_OFF, SPEED_OFF, SPEED_OFF, SPEED_OFF);
}

// ============================================================================
// LIGHTING & HORN VISUAL FEEDBACK (NO SOUND IN CAR)
// ============================================================================

void toggleHeadlight() {
  headlightOn = !headlightOn;
  if (!hornBlinkActive) {
    digitalWrite(PIN_HEADLIGHT, headlightOn ? HIGH : LOW);
  }
}

void toggleBacklight() {
  backlightOn = !backlightOn;
  digitalWrite(PIN_BACKLIGHT, backlightOn ? HIGH : LOW);
}

/*
 * When 'O' (Horn) is received, blink Headlight twice visually.
 * Car has NO speaker and remains completely silent.
 */
void triggerHornBlink() {
  hornBlinkActive = true;
  hornBlinkStep = 0;
  hornBlinkTimer = millis();
  digitalWrite(PIN_HEADLIGHT, HIGH);
}

void handleHornHeadlightBlink() {
  if (!hornBlinkActive) return;

  unsigned long now = millis();
  if (now - hornBlinkTimer >= 120) {
    hornBlinkTimer = now;
    hornBlinkStep++;

    if (hornBlinkStep == 1) {
      digitalWrite(PIN_HEADLIGHT, LOW);   // Blink 1 OFF
    } else if (hornBlinkStep == 2) {
      digitalWrite(PIN_HEADLIGHT, HIGH);  // Blink 2 ON
    } else if (hornBlinkStep == 3) {
      digitalWrite(PIN_HEADLIGHT, LOW);   // Blink 2 OFF
    } else {
      // Restore actual Headlight state
      hornBlinkActive = false;
      digitalWrite(PIN_HEADLIGHT, headlightOn ? HIGH : LOW);
    }
  }
}
