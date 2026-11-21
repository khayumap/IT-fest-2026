# TRI-CONTROL HUB — 3-in-1 Smart RC Car

A complete IT Fest hardware + web telemetry project combining a **Real-Car-Style RC Controller** (10K Potentiometer Steering + Joystick Accelerator + Bell Push Switch + 3 Ohm Speaker), an **Ultrasonic Radar Scanner**, and a **Hardware LCD Dino Game** — paired with a **Web Bluetooth Control Hub** ready for static HTTPS hosting on Render.

---

## Project Folder Structure

```text
TRI-CONTROL-HUB/
├── website/
│   ├── index.html
│   ├── style.css
│   └── script.js
├── code/
│   ├── joystick_controller.ino
│   └── car_receiver.ino
├── wiring.md
├── README.md
└── render.yaml
```

---

## 1. Backup Existing NodeMCU Firmware (`esptool.py`)

Before flashing `car_receiver.ino`, back up the existing NodeMCU flash memory so you can restore it anytime:

1. Install Python & `esptool`:
   ```bash
   pip install esptool
   ```
2. Find your NodeMCU COM port (Windows Device Manager, e.g., `COM5`, or Linux/macOS `/dev/ttyUSB0`).
3. Run `read_flash` to dump all 4MB of flash:
   ```bash
   esptool.py --port COM5 --baud 115200 read_flash 0x00000 0x400000 nodemcu_backup.bin
   ```
   *(To restore later: `esptool.py --port COM5 --baud 115200 write_flash 0x00000 nodemcu_backup.bin`)*

---

## 2. Upload Firmware to Hardware

### A. Upload `code/joystick_controller.ino` to Arduino UNO
1. Open Arduino IDE and select **Tools → Board → Arduino UNO**.
2. Select the Arduino UNO **Port** (`COMx` / `/dev/ttyACMx`).
3. **Important:** Temporarily unplug `D10` / `D11` if needed (optional for SoftwareSerial, recommended if HC-05 draws USB surge).
4. Open `code/joystick_controller.ino` and click **Upload**.
   - *Libraries used:* Only built-in `Wire.h`, `Servo.h`, and `SoftwareSerial.h` — no external downloads needed.

### B. Upload `code/car_receiver.ino` to NodeMCU ESP8266
1. Install ESP8266 Board Support in Arduino IDE (`http://arduino.esp8266.com/stable/package_esp8266com_index.json`).
2. Select **Tools → Board → NodeMCU 1.0 (ESP-12E Module)**.
3. **CRITICAL:** Disconnect the **HC-05 RX and TX** jumper wires from NodeMCU `RX` and `TX` before uploading! If HC-05 stays connected to Hardware Serial, upload will fail with `esptool.FatalError`.
4. Open `code/car_receiver.ino`, click **Upload**, and reconnect HC-05 RX/TX after upload completes.

---

## 3. Pair HC-05 Master (Joystick) with HC-05 Slave (Car)

1. **Power Car HC-05 (Slave)** in AT Command Mode (hold the tiny button on HC-05 while plugging in 5V — LED blinks slowly every 2 seconds):
   - Set baud `9600`: `AT+UART=9600,0,0`
   - Set Slave role: `AT+ROLE=0`
   - Set pairing password: `AT+PSWD="1234"`
   - Read Slave MAC address: `AT+ADDR?` (e.g., `+ADDR:98D3:31:FB1234` -> replace colons with commas: `98D3,31,FB1234`).
2. **Power Joystick HC-05 (Master)** in AT Command Mode:
   - Set baud `9600`: `AT+UART=9600,0,0`
   - Set Master role: `AT+ROLE=1`
   - Set connection mode to fixed address: `AT+CMODE=0`
   - Set same password: `AT+PSWD="1234"`
   - Bind to Car Slave address: `AT+BIND=98D3,31,FB1234`
3. Reboot both HC-05 modules normally. They will auto-pair within 3–5 seconds (both blink twice every 2 seconds), Car `D0` Green LED turns ON, and Joystick Speaker plays the **Happy Connect Melody**!

---

## 4. Connect the 3 Ohm Speaker (No Transistor) & Bell Switch

### 3 Ohm Speaker via 330 Ohm Resistor
- Wire **Arduino `D6` → `330 Ohm` Resistor → Speaker `(+)` Red**.
- Wire **Speaker `(-)` Black → Arduino `GND`**.
- **Never use 100 Ohm or direct connection:** A 3 Ohm speaker with 100 Ohm draws `48.5mA` (`> 40mA` pin limit) and damages `D6`. With `330 Ohm`, current is safely `15mA`.
- Place the speaker inside a small plastic cup to amplify volume cleanly without any amplifier IC or transistor.

### Bell Push Switch (Doorbell Switch)
- Wire one terminal of the Bell Push Switch to **Arduino `D2`**.
- Wire the second terminal to **Arduino `GND`**.
- Leave the small thumbstick `SW` pin unconnected. `D2` uses `INPUT_PULLUP` (Active LOW).

### 10K Potentiometer (Steering Wheel)
- Left outer pin → **`5V`**, Middle wiper pin → **`A2`**, Right outer pin → **`GND`**.

---

## 5. Testing Speaker Functions (2, 3, and 5)

1. **Test Function 5 (Bluetooth Connect / Disconnect Alert):**
   - Power on the Arduino UNO → Hear Startup Melody (`C-E-G-C`: 523, 659, 784, 1046 Hz).
   - Power on the Car → Once HC-05 pairs and sends `'C'`, hear Happy Connect Melody (`523-659-784 Hz`).
   - Unplug Car battery for 3 seconds → Hear Sad Disconnect Melody (`784-659-523 Hz`).
2. **Test Function 2 (Radar Sonar Beep):**
   - Hold **Master Button (`D9`) for 2 seconds** → Hear 2 short mode-change beeps; LCD switches to `MODE: RADAR`.
   - Hold your hand in front of HC-SR04 at `>30cm` (`SAFE`, 1000ms beep), `15-30cm` (`WARNING`, 500ms), `10-15cm` (`DANGER!`, 200ms), and `<10cm` (`STOP!!`, 100ms rapid beep).
3. **Test Function 3 (Dino Game Sound):**
   - Hold **Master Button (`D9`) for 2 seconds** again → Hear ascending Game Start melody; LCD launches Dino Game.
   - Press the **Bell Push Switch (`D2`)** (or push Joystick UP) → Dino jumps with `600Hz` Boing sound.
   - Clear a cactus → Hear `1200Hz` Ting (`+10` score).
   - Hit a cactus → LCD shows `GAME OVER!` and plays descending sad melody (`400 -> 300 -> 200 -> 100 Hz`).

---

## 6. Charging the 7.4V Lithium Battery Safely

- The Car uses **2x 18650 Li-Ion cells in series (7.4V nominal, 8.4V full)**.
- Always charge using a dedicated **2S (8.4V) Balance Li-Ion Charger** or **2S BMS USB-C charger board** — never use a 5V phone charger directly on the battery pack.
- Disconnect the battery from the L298N before charging, and never discharge below `6.0V` (`3.0V` per cell).

---

## 7. Running the Website & Connecting via Bluetooth

1. Open `website/index.html` in **Google Chrome** over **HTTPS** (or `http://localhost` for local testing). *Web Bluetooth API requires a Secure Context (`https://`).*
2. Click **CONNECT BLUETOOTH** on Tab 1 (**CONTROL DASHBOARD**).
3. Note on Bluetooth modules: Standard **HC-05** uses Bluetooth Classic SPP, whereas browser Web Bluetooth API (`navigator.bluetooth`) communicates with BLE GATT devices (such as HM-10 / ESP32 BLE). If HC-05 does not appear in the browser BLE picker, the website shows a helpful notice and allows full **Demo / Presentation Mode** while the hardware Arduino UNO Joystick remains the primary controller for the HC-05 car.

---

## 8. Hosting on Render (Static Site — `website/` Only)

This repository contains both `code/` and `website/`. Render is configured via `render.yaml` to publish **only** `./website` and ignore `code/` completely (no Node.js, no backend, no build step, pure relative paths `./style.css` and `./script.js`).

### Option A: Blueprint (`render.yaml`)
Push this repository to GitHub and click **New → Blueprint** in Render — it automatically detects `render.yaml`.

### Option B: Manual Render Static Site Setup
1. Log in to Render and click **New Static Site**.
2. **Select Repo** (`TRI-CONTROL-HUB`).
3. Set **Publish Directory** = `website`.
4. Leave **Build Command** = empty (`""`).
5. Click **Create Static Site**.
6. Open the generated `https://...onrender.com` URL in Chrome (HTTPS enabled automatically for Web Bluetooth).