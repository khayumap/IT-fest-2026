/* ============================================================================
 * TRI-CONTROL HUB — 3-in-1 Smart RC Car
 * Pure Vanilla JavaScript (Web Bluetooth + Web Audio + Radar + Dino Game)
 * ============================================================================ */

(() => {
  'use strict';

  // --------------------------------------------------------------------------
  // DOM REFERENCES
  // --------------------------------------------------------------------------
  const btnConnectBt = document.getElementById('btnConnectBt');
  const statusBt = document.getElementById('statusBt');
  const statusBtLed = document.getElementById('statusBtLed');
  const statusBtText = document.getElementById('statusBtText');
  const statusMode = document.getElementById('statusMode');
  const statusDistance = document.getElementById('statusDistance');

  const btAlertBanner = document.getElementById('btAlertBanner');
  const btAlertTitle = document.getElementById('btAlertTitle');
  const btAlertMessage = document.getElementById('btAlertMessage');
  const btAlertClose = document.getElementById('btAlertClose');

  const tabButtons = document.querySelectorAll('.tab-btn');
  const tabPanels = document.querySelectorAll('.tab-panel');

  // Tab 1: Control Dashboard
  const steeringSlider = document.getElementById('steeringSlider');
  const btnCenterSteer = document.getElementById('btnCenterSteer');
  const steeringWheel = document.getElementById('steeringWheel');
  const steerDirectionText = document.getElementById('steerDirectionText');
  const steerAdcValue = document.getElementById('steerAdcValue');

  const btnForward = document.getElementById('btnForward');
  const btnStop = document.getElementById('btnStop');
  const btnBackward = document.getElementById('btnBackward');
  const accelStateText = document.getElementById('accelStateText');
  const activeCommandBadge = document.getElementById('activeCommandBadge');
  const combinedLogicText = document.getElementById('combinedLogicText');
  const dirCells = document.querySelectorAll('.dir-cell');
  const lastTxLog = document.getElementById('lastTxLog');

  const btnHeadlight = document.getElementById('btnHeadlight');
  const btnBacklight = document.getElementById('btnBacklight');
  const btnHorn = document.getElementById('btnHorn');
  const headlightState = document.getElementById('headlightState');
  const backlightState = document.getElementById('backlightState');
  const headlightLamp = document.getElementById('headlightLamp');
  const backlightLamp = document.getElementById('backlightLamp');
  const legendGreenLed = document.getElementById('legendGreenLed');
  const legendRedLed = document.getElementById('legendRedLed');

  // Tab 2: Radar Scanner
  const btnSimulateObstacle = document.getElementById('btnSimulateObstacle');
  const btnClearRadar = document.getElementById('btnClearRadar');
  const radarTargetsLayer = document.getElementById('radarTargetsLayer');
  const radarReadout = document.getElementById('radarReadout');
  const radarDistVal = document.getElementById('radarDistVal');
  const radarAngleVal = document.getElementById('radarAngleVal');
  const radarZoneVal = document.getElementById('radarZoneVal');

  // Tab 3: Dino Game
  const dinoCanvas = document.getElementById('dinoCanvas');
  const dinoCtx = dinoCanvas.getContext('2d');
  const dinoScoreEl = document.getElementById('dinoScore');
  const dinoHighScoreEl = document.getElementById('dinoHighScore');
  const btnDinoJump = document.getElementById('btnDinoJump');
  const btnDinoRestart = document.getElementById('btnDinoRestart');

  // --------------------------------------------------------------------------
  // SHARED STATE
  // --------------------------------------------------------------------------
  let bluetoothDevice = null;
  let bluetoothCharacteristic = null;
  let isBtConnected = false;

  let currentAccel = 'STOP'; // 'FWD' | 'STOP' | 'BACK'
  let headlightOn = false;
  let backlightOn = false;
  let hornBlinkTimeoutIds = [];

  let audioCtx = null;

  // --------------------------------------------------------------------------
  // WEB AUDIO HELPER
  // --------------------------------------------------------------------------
  function getAudioContext() {
    if (!audioCtx) {
      const AudioContextClass = window.AudioContext || window.webkitAudioContext;
      if (AudioContextClass) {
        audioCtx = new AudioContextClass();
      }
    }
    if (audioCtx && audioCtx.state === 'suspended') {
      audioCtx.resume().catch(() => {});
    }
    return audioCtx;
  }

  function playTone(freq, durationMs, type = 'square', delayMs = 0) {
    const ctx = getAudioContext();
    if (!ctx) return;

    const startTime = ctx.currentTime + delayMs / 1000;
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();

    osc.type = type;
    osc.frequency.setValueAtTime(freq, startTime);

    gain.gain.setValueAtTime(0.12, startTime);
    gain.gain.exponentialRampToValueAtTime(0.001, startTime + durationMs / 1000);

    osc.connect(gain);
    gain.connect(ctx.destination);

    osc.start(startTime);
    osc.stop(startTime + durationMs / 1000 + 0.02);
  }

  // --------------------------------------------------------------------------
  // ALERT BANNER HELPER (Replaces blocking window.alert)
  // --------------------------------------------------------------------------
  function showAlert(title, message) {
    btAlertTitle.textContent = title;
    btAlertMessage.textContent = message;
    btAlertBanner.classList.remove('hidden');
  }

  btAlertClose.addEventListener('click', () => {
    btAlertBanner.classList.add('hidden');
  });

  // --------------------------------------------------------------------------
  // BLUETOOTH CONNECTION (Web Bluetooth API) & STATUS SYNC
  // --------------------------------------------------------------------------
  function setBluetoothUiState(connected, deviceName = '') {
    isBtConnected = connected;
    if (connected) {
      statusBt.classList.remove('status-disconnected');
      statusBt.classList.add('status-connected');
      statusBtLed.classList.remove('led-red');
      statusBtLed.classList.add('led-green');
      statusBtText.textContent = deviceName ? `CONNECTED (${deviceName})` : 'CONNECTED';

      legendGreenLed.classList.remove('led-green-dim');
      legendGreenLed.classList.add('led-green');
      legendRedLed.classList.remove('led-red');
      legendRedLed.classList.add('led-red-dim');
      btnConnectBt.textContent = 'DISCONNECT BT';
    } else {
      statusBt.classList.remove('status-connected');
      statusBt.classList.add('status-disconnected');
      statusBtLed.classList.remove('led-green');
      statusBtLed.classList.add('led-red');
      statusBtText.textContent = 'DISCONNECTED';

      legendGreenLed.classList.remove('led-green');
      legendGreenLed.classList.add('led-green-dim');
      legendRedLed.classList.remove('led-red-dim');
      legendRedLed.classList.add('led-red');
      btnConnectBt.textContent = 'CONNECT BLUETOOTH';
    }
  }

  async function connectBluetooth() {
    if (isBtConnected && bluetoothDevice && bluetoothDevice.gatt.connected) {
      bluetoothDevice.gatt.disconnect();
      setBluetoothUiState(false);
      return;
    }

    if (!navigator.bluetooth) {
      showAlert(
        'Web Bluetooth Unavailable',
        'Web Bluetooth API requires HTTPS and Google Chrome (Desktop or Android). Note: Standard HC-05 uses Classic Bluetooth, so the Hardware Arduino UNO Joystick is the main controller!'
      );
      return;
    }

    try {
      // Standard BLE UART / HM-10 / Nordic UART optional services
      const UART_SERVICES = [
        0xffe0,
        '0000ffe0-0000-1000-8000-00805f9b34fb',
        '6e400001-b5a3-f393-e0a9-e50e24dcca9e'
      ];

      bluetoothDevice = await navigator.bluetooth.requestDevice({
        acceptAllDevices: true,
        optionalServices: UART_SERVICES
      });

      bluetoothDevice.addEventListener('gattserverdisconnected', () => {
        bluetoothCharacteristic = null;
        setBluetoothUiState(false);
      });

      const server = await bluetoothDevice.gatt.connect();
      const services = await server.getPrimaryServices();

      for (const service of services) {
        const characteristics = await service.getCharacteristics();
        for (const char of characteristics) {
          if (char.properties.write || char.properties.writeWithoutResponse) {
            bluetoothCharacteristic = char;
          }
          if (char.properties.notify) {
            await char.startNotifications();
            char.addEventListener('characteristicvaluechanged', handleIncomingBtData);
          }
        }
      }

      setBluetoothUiState(true, bluetoothDevice.name || 'HC-05');
      btAlertBanner.classList.add('hidden');
      playTone(523, 120, 'triangle', 0);
      playTone(659, 120, 'triangle', 140);
      playTone(784, 220, 'triangle', 280);
    } catch (err) {
      setBluetoothUiState(false);
      showAlert(
        'Bluetooth Connection Failed',
        `Could not connect to HC-05 (${err.message}). Make sure you are on HTTPS in Google Chrome with Bluetooth enabled. Note: Standard HC-05 modules use Classic Bluetooth SPP, so the hardware Arduino UNO Joystick remains the primary controller.`
      );
    }
  }

  function handleIncomingBtData(event) {
    const decoder = new TextDecoder('utf-8');
    const raw = decoder.decode(event.target.value).trim();
    // Parse radar telemetry packet formatted like "D:25,A:90"
    const match = raw.match(/D:(\d+),A:(\d+)/i);
    if (match) {
      const distCm = parseInt(match[1], 10);
      const angleDeg = parseInt(match[2], 10);
      plotRadarObstacle(distCm, angleDeg);
    }
  }

  btnConnectBt.addEventListener('click', connectBluetooth);

  // --------------------------------------------------------------------------
  // COMMAND SENDER: sendCmd()
  // --------------------------------------------------------------------------
  async function sendCmd(cmd) {
    const cleanCmd = String(cmd).trim().toUpperCase();
    activeCommandBadge.textContent = cleanCmd;

    // Highlight active cell in 8-direction matrix
    dirCells.forEach((cell) => {
      cell.classList.toggle('active-dir', cell.dataset.cmd === cleanCmd);
    });

    const timestamp = new Date().toLocaleTimeString([], {
      hour12: false,
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });

    if (bluetoothCharacteristic) {
      try {
        const encoder = new TextEncoder();
        await bluetoothCharacteristic.writeValue(encoder.encode(`${cleanCmd}\n`));
        lastTxLog.textContent = `${cleanCmd} @ ${timestamp} (BT TX)`;
      } catch (e) {
        lastTxLog.textContent = `${cleanCmd} @ ${timestamp} (Local)`;
      }
    } else {
      lastTxLog.textContent = `${cleanCmd} @ ${timestamp} (Demo)`;
    }
  }

  // Expose sendCmd globally for inspection/testing
  window.sendCmd = sendCmd;

  // --------------------------------------------------------------------------
  // TAB NAVIGATION
  // --------------------------------------------------------------------------
  tabButtons.forEach((btn) => {
    btn.addEventListener('click', () => {
      const targetId = btn.dataset.tab;
      const modeName = btn.dataset.mode;

      tabButtons.forEach((b) => {
        const isActive = b === btn;
        b.classList.toggle('active', isActive);
        b.setAttribute('aria-selected', String(isActive));
      });

      tabPanels.forEach((panel) => {
        const isTarget = panel.id === targetId;
        panel.classList.toggle('active', isTarget);
        panel.hidden = !isTarget;
      });

      statusMode.textContent = modeName;

      // Safety: Stop RC car when switching away from Control Dashboard
      if (targetId !== 'tabControl' && currentAccel !== 'STOP') {
        currentAccel = 'STOP';
        updateDriveOutput(true);
      }

      if (targetId === 'tabDino') {
        drawDinoScene();
      }
    });
  });

  // --------------------------------------------------------------------------
  // TAB 1: POTENTIOMETER STEERING + ACCELERATOR LOGIC
  // --------------------------------------------------------------------------
  function getSteeringZone(adcVal) {
    if (adcVal < 400) return 'LEFT';
    if (adcVal <= 620) return 'STRAIGHT';
    return 'RIGHT';
  }

  function computeCombinedCommand(steerZone, accelState) {
    if (accelState === 'FWD') {
      if (steerZone === 'LEFT') return 'FL';
      if (steerZone === 'RIGHT') return 'FR';
      return 'F';
    }
    if (accelState === 'BACK') {
      if (steerZone === 'LEFT') return 'BL';
      if (steerZone === 'RIGHT') return 'BR';
      return 'B';
    }
    // Stationary / Joystick Middle
    if (steerZone === 'LEFT') return 'L';
    if (steerZone === 'RIGHT') return 'R';
    return 'S';
  }

  function updateSteeringVisuals() {
    const val = parseInt(steeringSlider.value, 10);
    const zone = getSteeringZone(val);

    steerAdcValue.textContent = String(val);
    steerDirectionText.textContent = zone;

    // Map 0..1023 -> -48deg..+48deg
    const deg = ((val - 512) / 512) * 48;
    steeringWheel.style.transform = `rotate(${deg.toFixed(1)}deg)`;
    return zone;
  }

  function updateDriveOutput(shouldTransmit = true) {
    const steerZone = updateSteeringVisuals();
    const cmd = computeCombinedCommand(steerZone, currentAccel);

    accelStateText.textContent = currentAccel;
    combinedLogicText.textContent = `Steering ${steerZone} + ${currentAccel} = ${cmd}`;

    if (shouldTransmit) {
      sendCmd(cmd);
    }
  }

  steeringSlider.addEventListener('input', () => {
    updateDriveOutput(true);
  });

  btnCenterSteer.addEventListener('click', () => {
    steeringSlider.value = '512';
    updateDriveOutput(true);
  });

  // Press-and-hold Forward & Backward buttons
  function bindHoldButton(buttonEl, accelMode) {
    const startHold = (e) => {
      e.preventDefault();
      currentAccel = accelMode;
      buttonEl.classList.add('is-pressed');
      updateDriveOutput(true);
    };

    const endHold = () => {
      if (currentAccel === accelMode) {
        currentAccel = 'STOP';
        buttonEl.classList.remove('is-pressed');
        accelStateText.textContent = 'STOP';
        const steerZone = getSteeringZone(parseInt(steeringSlider.value, 10));
        combinedLogicText.textContent = `Steering ${steerZone} + STOP = S`;
        sendCmd('S');
      }
    };

    buttonEl.addEventListener('pointerdown', startHold);
    buttonEl.addEventListener('pointerup', endHold);
    buttonEl.addEventListener('pointerleave', endHold);
    buttonEl.addEventListener('pointercancel', endHold);
  }

  bindHoldButton(btnForward, 'FWD');
  bindHoldButton(btnBackward, 'BACK');

  btnStop.addEventListener('click', () => {
    currentAccel = 'STOP';
    btnForward.classList.remove('is-pressed');
    btnBackward.classList.remove('is-pressed');
    accelStateText.textContent = 'STOP';
    const steerZone = getSteeringZone(parseInt(steeringSlider.value, 10));
    combinedLogicText.textContent = `Steering ${steerZone} + STOP = S`;
    sendCmd('S');
  });

  // Headlight Toggle (D2)
  function renderHeadlightLamp(isOn) {
    headlightLamp.classList.toggle('lamp-head-on', isOn);
    headlightLamp.classList.toggle('lamp-head-off', !isOn);
  }

  btnHeadlight.addEventListener('click', () => {
    headlightOn = !headlightOn;
    btnHeadlight.classList.toggle('is-on', headlightOn);
    btnHeadlight.setAttribute('aria-pressed', String(headlightOn));
    headlightState.textContent = headlightOn ? 'ON (D2)' : 'OFF (D2)';
    renderHeadlightLamp(headlightOn);
    sendCmd('H');
  });

  // Backlight Toggle (D3)
  btnBacklight.addEventListener('click', () => {
    backlightOn = !backlightOn;
    btnBacklight.classList.toggle('is-on', backlightOn);
    btnBacklight.setAttribute('aria-pressed', String(backlightOn));
    backlightState.textContent = backlightOn ? 'ON (D3)' : 'OFF (D3)';
    backlightLamp.classList.toggle('lamp-back-on', backlightOn);
    backlightLamp.classList.toggle('lamp-back-off', !backlightOn);
    sendCmd('T');
  });

  // Horn Button (Sends O + Web Audio 800Hz beep + flashes Headlight twice)
  btnHorn.addEventListener('click', () => {
    sendCmd('O');
    playTone(800, 200, 'square', 0);

    // Clear any in-flight blink sequence
    hornBlinkTimeoutIds.forEach((id) => clearTimeout(id));
    hornBlinkTimeoutIds = [];

    // Flash Headlight indicator twice matching Car NodeMCU D2 blink
    renderHeadlightLamp(true);
    hornBlinkTimeoutIds.push(setTimeout(() => renderHeadlightLamp(false), 120));
    hornBlinkTimeoutIds.push(setTimeout(() => renderHeadlightLamp(true), 240));
    hornBlinkTimeoutIds.push(setTimeout(() => renderHeadlightLamp(false), 360));
    hornBlinkTimeoutIds.push(setTimeout(() => renderHeadlightLamp(headlightOn), 480));
  });

  // Keyboard shortcuts for Desktop Driving (ArrowUp / ArrowDown / ArrowLeft / ArrowRight)
  window.addEventListener('keydown', (e) => {
    const controlActive = document.getElementById('tabControl').classList.contains('active');
    if (!controlActive || e.repeat) return;

    if (e.key === 'ArrowUp' || e.key === 'w' || e.key === 'W') {
      currentAccel = 'FWD';
      btnForward.classList.add('is-pressed');
      updateDriveOutput(true);
    } else if (e.key === 'ArrowDown' || e.key === 's' || e.key === 'S') {
      currentAccel = 'BACK';
      btnBackward.classList.add('is-pressed');
      updateDriveOutput(true);
    } else if (e.key === 'ArrowLeft' || e.key === 'a' || e.key === 'A') {
      steeringSlider.value = '180';
      updateDriveOutput(true);
    } else if (e.key === 'ArrowRight' || e.key === 'd' || e.key === 'D') {
      steeringSlider.value = '840';
      updateDriveOutput(true);
    }
  });

  window.addEventListener('keyup', (e) => {
    const controlActive = document.getElementById('tabControl').classList.contains('active');
    if (!controlActive) return;

    if (e.key === 'ArrowUp' || e.key === 'w' || e.key === 'W' || e.key === 'ArrowDown' || e.key === 's' || e.key === 'S') {
      currentAccel = 'STOP';
      btnForward.classList.remove('is-pressed');
      btnBackward.classList.remove('is-pressed');
      accelStateText.textContent = 'STOP';
      sendCmd('S');
    } else if (e.key === 'ArrowLeft' || e.key === 'a' || e.key === 'A' || e.key === 'ArrowRight' || e.key === 'd' || e.key === 'D') {
      steeringSlider.value = '512';
      updateDriveOutput(true);
    }
  });

  // Initialize Steering visuals on load
  updateSteeringVisuals();

  // --------------------------------------------------------------------------
  // TAB 2: LIVE RADAR SCANNER
  // --------------------------------------------------------------------------
  function getZoneDetails(distanceCm) {
    if (distanceCm < 10) return { label: 'STOP!!', className: 'zone-stop' };
    if (distanceCm <= 15) return { label: 'DANGER!', className: 'zone-danger' };
    if (distanceCm <= 30) return { label: 'WARNING', className: 'zone-warn' };
    return { label: 'SAFE', className: 'zone-safe' };
  }

  function plotRadarObstacle(distanceCm, angleDeg) {
    // Map distance (0-60cm) to radius inside 300x300 radar (center=150, maxRadius=134)
    const clampedDist = Math.max(3, Math.min(60, distanceCm));
    const radiusPx = (clampedDist / 60) * 134;
    const rad = ((angleDeg - 90) * Math.PI) / 180;

    const x = 150 + radiusPx * Math.cos(rad);
    const y = 150 + radiusPx * Math.sin(rad);

    // Keep at most 8 recent red obstacle dots
    while (radarTargetsLayer.children.length >= 8) {
      radarTargetsLayer.removeChild(radarTargetsLayer.firstChild);
    }

    const dot = document.createElement('div');
    dot.className = 'obstacle-dot';
    dot.style.left = `${x.toFixed(1)}px`;
    dot.style.top = `${y.toFixed(1)}px`;
    dot.title = `${distanceCm} cm @ ${angleDeg}°`;
    radarTargetsLayer.appendChild(dot);

    // Update Telemetry Text + Top Status Bar Distance
    const zone = getZoneDetails(distanceCm);
    radarReadout.textContent = `Distance: ${distanceCm} cm | Direction: Obstacle at ~${angleDeg}°`;
    radarDistVal.textContent = `${distanceCm} cm`;
    radarAngleVal.textContent = `${angleDeg}°`;
    radarZoneVal.textContent = zone.label;
    radarZoneVal.className = `metric-big ${zone.className}`;

    statusDistance.textContent = `${distanceCm} cm`;
  }

  btnSimulateObstacle.addEventListener('click', () => {
    const randomDist = Math.floor(Math.random() * 52) + 5; // 5cm - 56cm
    const randomAngle = Math.floor(Math.random() * 181); // 0 - 180 deg sweep sector
    plotRadarObstacle(randomDist, randomAngle);
  });

  btnClearRadar.addEventListener('click', () => {
    radarTargetsLayer.innerHTML = '';
    radarReadout.textContent = 'Distance: -- cm | Direction: Waiting for sweep...';
    radarDistVal.textContent = '-- cm';
    radarAngleVal.textContent = '--°';
    radarZoneVal.textContent = 'IDLE';
    radarZoneVal.className = 'metric-big';
    statusDistance.textContent = '-- cm';
  });

  // --------------------------------------------------------------------------
  // TAB 3: DINO GAME (600x200 Canvas + Web Audio + localStorage High Score)
  // --------------------------------------------------------------------------
  const HIGH_SCORE_KEY = 'tri_control_dino_highscore';
  let highScore = parseInt(localStorage.getItem(HIGH_SCORE_KEY) || '0', 10);
  dinoHighScoreEl.textContent = String(highScore);

  const GROUND_Y = 166;
  const DINO_X = 56;
  const DINO_SIZE = 34;

  let dinoState = {
    y: GROUND_Y - DINO_SIZE,
    vy: 0,
    gravity: 0.72,
    jumpForce: -12.6,
    isJumping: false,
    isGameOver: false,
    score: 0,
    speed: 5.2,
    cacti: [{ x: 620, width: 24, height: 38, scored: false }],
    animFrameId: null
  };

  function resetDinoGame() {
    if (dinoState.animFrameId) {
      cancelAnimationFrame(dinoState.animFrameId);
    }

    dinoState = {
      y: GROUND_Y - DINO_SIZE,
      vy: 0,
      gravity: 0.72,
      jumpForce: -12.6,
      isJumping: false,
      isGameOver: false,
      score: 0,
      speed: 5.2,
      cacti: [{ x: 620, width: 24, height: 38, scored: false }],
      animFrameId: null
    };

    dinoScoreEl.textContent = '0';
    btnDinoRestart.classList.add('hidden');
    btnDinoJump.disabled = false;

    // Ascending start sound
    playTone(523, 120, 'triangle', 0);
    playTone(659, 120, 'triangle', 140);
    playTone(784, 220, 'triangle', 280);

    dinoLoop();
  }

  function triggerDinoJump() {
    if (dinoState.isGameOver) return;
    if (!dinoState.isJumping) {
      dinoState.isJumping = true;
      dinoState.vy = dinoState.jumpForce;
      playTone(600, 150, 'square', 0); // Boing sound matching Arduino Function 3
    }
  }

  function playGameOverBuzzer() {
    playTone(400, 180, 'sawtooth', 0);
    playTone(300, 180, 'sawtooth', 200);
    playTone(200, 180, 'sawtooth', 400);
    playTone(100, 360, 'sawtooth', 600);
  }

  function drawDinoScene() {
    const ctx = dinoCtx;
    ctx.clearRect(0, 0, dinoCanvas.width, dinoCanvas.height);

    // Crisp White Background
    ctx.fillStyle = '#ffffff';
    ctx.fillRect(0, 0, dinoCanvas.width, dinoCanvas.height);

    // Ground Line
    ctx.strokeStyle = '#334155';
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.moveTo(0, GROUND_Y);
    ctx.lineTo(dinoCanvas.width, GROUND_Y);
    ctx.stroke();

    // Subtle Ground Pebbles
    ctx.fillStyle = '#94a3b8';
    ctx.fillRect(80, GROUND_Y + 8, 12, 2);
    ctx.fillRect(240, GROUND_Y + 12, 18, 2);
    ctx.fillRect(430, GROUND_Y + 7, 10, 2);

    // Draw Dino Emoji
    ctx.save();
    ctx.font = '32px "Apple Color Emoji", "Segoe UI Emoji", sans-serif';
    ctx.textBaseline = 'top';
    // Flip horizontally so Dino faces right towards incoming cacti
    ctx.translate(DINO_X + DINO_SIZE / 2, dinoState.y);
    ctx.scale(-1, 1);
    ctx.fillText('🦖', -DINO_SIZE / 2, 0);
    ctx.restore();

    // Draw Green Cactus Obstacles
    dinoState.cacti.forEach((cactus) => {
      const cx = cactus.x;
      const cy = GROUND_Y - cactus.height;

      ctx.fillStyle = '#15803d';
      // Main trunk
      ctx.fillRect(cx + 7, cy, 10, cactus.height);
      // Left arm
      ctx.fillRect(cx, cy + 10, 7, 6);
      ctx.fillRect(cx, cy + 4, 5, 10);
      // Right arm
      ctx.fillRect(cx + 17, cy + 14, 7, 6);
      ctx.fillRect(cx + 19, cy + 7, 5, 11);
    });

    // Draw GAME OVER overlay on Canvas if collided
    if (dinoState.isGameOver) {
      ctx.fillStyle = 'rgba(5, 10, 20, 0.72)';
      ctx.fillRect(0, 0, dinoCanvas.width, dinoCanvas.height);

      ctx.fillStyle = '#ff3b5c';
      ctx.font = '700 32px "Chakra Petch", sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText('GAME OVER', dinoCanvas.width / 2, 82);

      ctx.fillStyle = '#f8fafc';
      ctx.font = '600 16px "JetBrains Mono", monospace';
      ctx.fillText(`FINAL SCORE: ${dinoState.score}`, dinoCanvas.width / 2, 118);
      ctx.textAlign = 'left';
    }
  }

  function dinoLoop() {
    if (dinoState.isGameOver) {
      drawDinoScene();
      return;
    }

    // Physics update
    dinoState.vy += dinoState.gravity;
    dinoState.y += dinoState.vy;

    if (dinoState.y >= GROUND_Y - DINO_SIZE) {
      dinoState.y = GROUND_Y - DINO_SIZE;
      dinoState.vy = 0;
      dinoState.isJumping = false;
    }

    // Move & Spawn Cacti
    for (let i = 0; i < dinoState.cacti.length; i++) {
      const c = dinoState.cacti[i];
      c.x -= dinoState.speed;

      // Score +10 when cactus passes Dino cleanly
      if (!c.scored && c.x + c.width < DINO_X) {
        c.scored = true;
        dinoState.score += 10;
        dinoScoreEl.textContent = String(dinoState.score);
        playTone(1200, 50, 'sine', 0); // Ting sound

        if (dinoState.score > highScore) {
          highScore = dinoState.score;
          dinoHighScoreEl.textContent = String(highScore);
          localStorage.setItem(HIGH_SCORE_KEY, String(highScore));
        }

        // Gradual speed ramp
        if (dinoState.speed < 9.5) {
          dinoState.speed += 0.15;
        }
      }

      // Collision Hitbox (forgiving 6px inset)
      const dinoBox = {
        x: DINO_X + 6,
        y: dinoState.y + 4,
        w: DINO_SIZE - 12,
        h: DINO_SIZE - 6
      };
      const cactusBox = {
        x: c.x + 4,
        y: GROUND_Y - c.height + 4,
        w: c.width - 8,
        h: c.height - 4
      };

      const collided =
        dinoBox.x < cactusBox.x + cactusBox.w &&
        dinoBox.x + dinoBox.w > cactusBox.x &&
        dinoBox.y < cactusBox.y + cactusBox.h &&
        dinoBox.y + dinoBox.h > cactusBox.y;

      if (collided) {
        dinoState.isGameOver = true;
        btnDinoRestart.classList.remove('hidden');
        btnDinoJump.disabled = true;
        playGameOverBuzzer();
        drawDinoScene();
        return;
      }
    }

    // Recycle cactus off-screen
    if (dinoState.cacti[0].x + dinoState.cacti[0].width < 0) {
      const nextHeight = 30 + Math.floor(Math.random() * 18);
      const nextGap = 620 + Math.floor(Math.random() * 140);
      dinoState.cacti[0] = {
        x: nextGap,
        width: 24,
        height: nextHeight,
        scored: false
      };
    }

    drawDinoScene();
    dinoState.animFrameId = requestAnimationFrame(dinoLoop);
  }

  btnDinoJump.addEventListener('click', triggerDinoJump);
  dinoCanvas.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    if (dinoState.isGameOver) {
      resetDinoGame();
    } else {
      triggerDinoJump();
    }
  });
  btnDinoRestart.addEventListener('click', resetDinoGame);

  // Spacebar Jump when Dino tab is active
  window.addEventListener('keydown', (e) => {
    const dinoActive = document.getElementById('tabDino').classList.contains('active');
    if (!dinoActive) return;

    if (e.code === 'Space') {
      e.preventDefault();
      if (dinoState.isGameOver) {
        resetDinoGame();
      } else {
        triggerDinoJump();
      }
    }
  });

  // Start Dino game loop cleanly on load
  dinoLoop();
})();
