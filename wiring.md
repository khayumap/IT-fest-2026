# TRI-CONTROL HUB — Complete Hardware Wiring Guide

This document defines the exact pin mappings, power architecture, chassis layout, and electrical safety rules for both the **Car Receiver (NodeMCU ESP8266)** and the **Joystick Controller (Arduino UNO)**.

---

## 1. Car Side Wiring — NodeMCU ESP8266 (DO NOT CHANGE)

**Car Hardware:** NodeMCU ESP8266 + L298N Motor Driver + 2 DC Motors in **FRONT** (Left + Right Drive Wheels) + 1 Caster Wheel in **BACK** + 7.4V Lithium Rechargeable Battery (2x 18650) + HC-05 Bluetooth Slave Module.  
**Important:** **NO Speaker** and **NO Buzzer** in the Car. The Car is completely silent; all audio feedback lives on the Joystick Controller.

### NodeMCU Pin Map

| NodeMCU Pin | Connected Device | Function |
| :--- | :--- | :--- |
| `D2` | Headlight (+) | Front Light Output via 220 Ohm resistor, LED to GND |
| `D3` | Backlight (+) | Rear Light Output via 220 Ohm resistor, LED to GND |
| `D5` | L298N Driver (IN1) | Left Motor Forward PWM |
| `D6` | L298N Driver (IN2) | Left Motor Reverse PWM |
| `D7` | L298N Driver (IN3) | Right Motor Forward PWM |
| `D8` | L298N Driver (IN4) | Right Motor Reverse PWM |
| `D0` | Status LED Green (+) | Bluetooth Connected Indicator, via 220 Ohm resistor to GND |
| `D1` | Status LED Red (+) | Bluetooth Disconnected Indicator, via 220 Ohm resistor to GND |
| `A0` | HC-05 STATE Pin | Real-time Connection Telemetry Monitoring (`HIGH` = Connected, `LOW` = Disconnected) |
| `RX` | HC-05 TX Pin | Bluetooth Receive — HC-05 TX connects directly to NodeMCU RX |
| `TX` | HC-05 RX Pin | Bluetooth Transmit — NodeMCU TX connects to HC-05 RX via voltage divider (1k Ohm + 2k Ohm) |
| `GND` | Common Ground Bus | Shared electrical ground for NodeMCU, L298N, HC-05, LEDs, and Battery (-) |
| `VIN` | L298N 5V Output | Regulated 5V power input for NodeMCU from L298N 5V regulator |
| `L298N 12V Input` | Lithium Battery (+) 7.4V | Main power input for DC drive motors |
| `L298N GND` | Lithium Battery (-) + NodeMCU GND | Common ground reference |

### Car Power Wiring Diagram

```
  [7.4V Li-Ion Battery (2x 18650)]
     │ (+) 7.4V                  │ (-) GND
     ▼                           ▼
  [L298N +12V Terminal]       [L298N GND] ◄────────────────────┐
     │                                                         │
     ├──► Internal 5V Regulator (Keep 5V-EN Jumper ON)          │
     │      │                                                  │
     │      ▼                                                  │
     │   [L298N +5V Output] ──► [NodeMCU VIN]                  │
     │                      └──► [HC-05 VCC (5V)]              │
     │                                                         │
     └──► Motor Channels (OUT1/OUT2 -> Front Left Motor)       │
          Motor Channels (OUT3/OUT4 -> Front Right Motor)      │
                                                               │
  [NodeMCU GND] + [HC-05 GND] + [All LED (-)] ─────────────────┘
```

> **TX/RX Voltage Divider Note:** NodeMCU outputs `3.3V` logic on `TX`. Using a `1k Ohm` resistor in series from `TX` to `HC-05 RX` and a `2k Ohm` resistor from `HC-05 RX` to `GND` protects the serial line cleanly while keeping reliable logic levels. Always **unplug HC-05 RX/TX wires when flashing NodeMCU** via USB.

### Chassis & Caster Wheel Placement

```
          FRONT OF CAR (Driving End)
   ┌─────────────────────────────────────┐
   │  [Front-Left Wheel]  [Front-Right]  │  <-- 2 DC Motors (L298N OUT1-4)
   │      (D5 / D6)         (D7 / D8)    │
   │                                     │
   │        [NodeMCU + L298N + 7.4V]     │
   │                                     │
   │            [Caster Wheel]           │  <-- 1 Free-Swiveling Support Wheel
   └─────────────────────────────────────┘
           REAR OF CAR (Support End)
```

### Car Movement Logic (Front-Wheel Differential Drive)

| Command | Movement | D5 (L-Fwd) | D6 (L-Rev) | D7 (R-Fwd) | D8 (R-Rev) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `F` | Forward | HIGH (255) | LOW (0) | HIGH (255) | LOW (0) |
| `B` | Backward | LOW (0) | HIGH (255) | LOW (0) | HIGH (255) |
| `L` | Spin Left | LOW (0) | LOW (0) | HIGH (255) | LOW (0) |
| `R` | Spin Right | HIGH (255) | LOW (0) | LOW (0) | LOW (0) |
| `FL` | Forward-Left | HALF (128) | LOW (0) | FULL (255) | LOW (0) |
| `FR` | Forward-Right | FULL (255) | LOW (0) | HALF (128) | LOW (0) |
| `BL` | Backward-Left | LOW (0) | HALF (128) | LOW (0) | FULL (255) |
| `BR` | Backward-Right | LOW (0) | FULL (255) | LOW (0) | HALF (128) |
| `S` | Stop | LOW (0) | LOW (0) | LOW (0) | LOW (0) |
| `H` | Headlight Toggle | `D2` toggles HIGH / LOW | — | — | — |
| `T` | Backlight Toggle | `D3` toggles HIGH / LOW | — | — | — |
| `O` | Horn (Visual Only) | `D2` blinks twice (NO sound in Car) | — | — | — |

**Bluetooth Telemetry LED Logic:**
- `A0 == HIGH` → `D0` Green LED **ON**, `D1` Red LED **OFF**
- `A0 == LOW` → `D0` Green LED **OFF**, `D1` Red LED **ON**

---

## 2. Joystick Side Wiring — Arduino UNO (Updated with Potentiometer Steering)

**Joystick Hardware:** Arduino UNO + Analog Joystick Module + 10K Potentiometer (B10K) + I2C LCD AIP31068 16x2 + Ultrasonic HC-SR04 + SG90 Servo + 3 Ohm 0.5W Mini Speaker + Bell Push Switch + Button1 (Headlight) + Button2 (Backlight) + Master Button + HC-05 Bluetooth Master + 9V Battery + Breadboard.

### Real-Car Control Concept (Potentiometer + Joystick)

- **10K Potentiometer (`A2`) = Steering Wheel:**
  - `0 – 400` → **LEFT**
  - `401 – 620` → **STRAIGHT** (Deadzone)
  - `621 – 1023` → **RIGHT**
- **Joystick Y-Axis (`A1`) = Accelerator Only:**
  - Push **UP** → Forward (`F` / `FL` / `FR`)
  - Center **Middle** → Coast/Spin/Stop (`S` / `L` / `R`)
  - Pull **DOWN** → Backward (`B` / `BL` / `BR`)
- **Joystick X-Axis (`A0`) & Joystick SW:** **NOT USED** in code.

### Arduino UNO Pin Map

| Arduino UNO Pin | Connected Device | Function |
| :--- | :--- | :--- |
| `A0` | Joystick VRx | **NOT USED** — Ignored in code |
| `A1` | Joystick VRy | Y-Axis Analog — **Accelerator Only** (Forward / Backward) |
| `A2` | Potentiometer Middle Pin (Wiper) | **Steering Wheel** (`0–400` Left, `401–620` Straight, `621–1023` Right) |
| `D2` | Bell Push Switch (Doorbell Switch) | Horn + Dino Jump — Active LOW (`INPUT_PULLUP`), one leg to `D2`, other to `GND`. Do NOT use Joystick SW |
| `A4` | LCD AIP31068 SDA | I2C Data |
| `A5` | LCD AIP31068 SCL | I2C Clock |
| `D3` | Ultrasonic HC-SR04 Trig | Trigger Output |
| `D4` | Ultrasonic HC-SR04 Echo | Echo Input |
| `D5` | SG90 Servo Signal | Radar Servo `0–180` Degrees |
| `D6` | 3 Ohm Mini Speaker (+) via 330 Ohm Resistor | Horn, Radar Sonar Beep, Dino Game Sound, Bluetooth Alert, Mode Change Melody (NO Transistor) |
| `D7` | Button1 — Headlight | Push Button to `GND` — Toggles Car Headlight (`H`) |
| `D8` | Button2 — Backlight | Push Button to `GND` — Toggles Car Backlight (`T`) |
| `D9` | Master Button | Push Button to `GND` — Long Press 2 Sec = Change Mode |
| `D10` | HC-05 TX (`SoftwareSerial RX`) | Arduino receives Bluetooth data from HC-05 TX |
| `D11` | HC-05 RX (`SoftwareSerial TX`) | Arduino transmits Bluetooth commands to HC-05 RX |
| `5V` | Joystick VCC, LCD VCC, Ultrasonic VCC, Servo VCC, Pot Left Pin | Regulated 5V Power Rail |
| `GND` | Common Ground | All Buttons, Bell Switch, Joystick GND, LCD GND, Ultrasonic GND, Servo GND, Speaker (-), HC-05 GND, Pot Right Pin |
| `VIN` | 9V Battery (+) | Power Input for Arduino UNO |
| `GND` | 9V Battery (-) | Battery Ground |

---

## 3. Critical Speaker Safety: Why 330 Ohm Resistor (Not 100 Ohm)

The speaker salvaged from a headphone or toy is a **3 Ohm, 0.5W** dynamic speaker — NOT an 8 Ohm speaker or piezo buzzer.

- **Direct connection to `D6`:** $I = \frac{5\text{V}}{3\,\Omega} \approx 1.67\text{A}$ → **Instantly burns the Arduino ATmega328P pin.**
- **100 Ohm resistor:** $I = \frac{5\text{V}}{100\,\Omega + 3\,\Omega} \approx 48.5\text{mA}$ → **Exceeds the Arduino UNO absolute max pin limit of 40mA.** Pin will overheat and fail. **DO NOT USE 100 Ohm.**
- **330 Ohm resistor (Required):** $I = \frac{5\text{V}}{330\,\Omega + 3\,\Omega} \approx 15.0\text{mA}$ → **100% safe** (well below 40mA, inside Arduino's recommended 20mA operating limit).

```
  Arduino UNO [D6] ───[ 330 Ohm Resistor ]───► (+) Red [3 Ohm 0.5W Speaker] Black (-) ───► [GND]
```

- **No Transistor / No PAM8403 / No DFPlayer:** Only the single `330 Ohm` series resistor is needed.
- **Acoustic Volume Tip:** Place the 3 Ohm speaker face-up inside a small plastic cup or bottle cap to act as a natural acoustic horn!

---

## 4. Bell Push Switch vs. Joystick SW Button

- **Small Joystick SW Button:** Built directly onto the moving thumbstick shaft; stiff to press vertically, frequently misses clicks when tilted, and easily fatigues or breaks during live judge demos.
- **Big Bell Push Switch (Doorbell Switch):** Heavy-duty spring-loaded panel switch wired between `D2` and `GND` with `INPUT_PULLUP`. Gives a crisp tactile **CLICK**, zero missed presses, and lets judges easily honk the horn or jump in the Dino Game without disturbing steering or throttle.
