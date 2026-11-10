# Nano Servo Shutter

A browser-controlled servo shutter adapted for an Arduino Nano (ATmega328P), using the original Servo-Shutter 3D-printed mechanism and SG90-compatible 9 g servo.

## Project status

This is a practical adaptation for timed holography / photographic exposures. It provides a USB serial connection from a desktop browser to the Nano. The classic Nano has no Wi-Fi; the web page uses the browser's Web Serial API.

**Important timing limitation:** the exposure value is the interval between the firmware's OPEN command and its later CLOSE command. It is not a guarantee of the same-duration optical exposure. The servo takes time to travel, and its motion varies with load, supply voltage, mechanics and angle. Calibrate the actual light transmission with a photodiode and oscilloscope before relying on exposure times. A small hobby servo is not a precision high-speed shutter.

## Hardware

- Arduino Nano, classic ATmega328P / 5 V
- SG90 or compatible 9 g positional servo
- Separate regulated 5 V supply for the servo, rated for at least 1 A (more headroom is useful)
- Common ground between the servo supply and Nano
- Original Servo-Shutter 3D-printed mechanical parts

### Wiring

| Servo lead | Connect to |
|---|---|
| Signal (usually orange/yellow) | Nano D9 |
| +5 V (usually red) | External regulated +5 V servo supply |
| GND (usually brown/black) | External supply GND and Nano GND |

Power the Nano from USB. Do **not** power the servo from the Nano's 5 V pin. Connect the external servo supply ground to Nano GND so the signal has a shared reference. Check your servo's actual wire colors before connecting.

## Firmware

Open `firmware/NanoServoShutter/NanoServoShutter.ino` in Arduino IDE. Select **Arduino Nano** and the correct processor/bootloader option for your board. Upload over USB.

The firmware starts with the shutter commanded to its configured closed position. Mechanical geometry may require adjusting the open and closed angles.

## Browser interface

Use a Chromium-based browser with Web Serial support (for example, desktop Chrome or Edge). The page must be served from localhost or HTTPS; opening the HTML file directly may disable serial access.

From the project root, run:

```bash
python -m http.server 8000
```

Then open [http://localhost:8000/web/](http://localhost:8000/web/), connect the Nano's USB serial port, and use the controls.

Close other applications that may already have the serial port open, including Arduino IDE Serial Monitor.

## Arduino Nano ESP32 Wi-Fi version (iPhone control)

The repository also includes a separate Wi-Fi access-point version for the **Arduino Nano ESP32**. This is independent of the classic Nano / USB Serial version described above.

### Requirements

- Arduino Nano ESP32
- SG90-compatible 9 g positional servo
- Separate regulated 5 V supply for the servo (at least 1 A recommended)
- Arduino IDE with the **Arduino ESP32 Boards** platform installed
- **ESP32Servo** library (install through Arduino IDE Library Manager)

### Upload

1. Open `firmware/ESP32WiFiShutter/ESP32WiFiShutter.ino` in Arduino IDE.
2. Select **Arduino Nano ESP32** under Tools → Board.
3. Select the board's USB port under Tools → Port.
4. Install **ESP32Servo** through Library Manager if it is not already installed.
5. Verify, then upload the sketch.
6. Optional: open Serial Monitor at 115200 baud. The board prints its access-point name and address at startup.

The servo signal is connected to **D9**. The sketch uses the Arduino `D9` pin alias so it works with either Nano ESP32 pin-numbering mode.

### iPhone Wi-Fi connection

The Nano ESP32 creates its own Wi-Fi network; a router or internet connection is not required.

1. Power the Nano ESP32.
2. On the iPhone, open **Settings → Wi-Fi**.
3. Join **HoloShutter-ESP32**.
4. The default password is **HoloShutter530**. Change `AP_PASSWORD` in the sketch before uploading if you want a different password (use at least 8 characters).
5. Open Safari and navigate to **http://192.168.4.1**.
6. If iOS reports “No Internet Connection,” remain connected to the shutter network; that is expected.

The web interface is embedded in the firmware, so no separate HTML upload or hosting step is needed.

### Wi-Fi controls and presets

The page provides manual OPEN, CLOSE and CANCEL controls; timed exposure; and open/closed servo-angle calibration. Presets are 2 minutes (120,000 ms), 2.5 minutes (150,000 ms), 3 minutes (180,000 ms), 3.5 minutes (210,000 ms), and 0.5 second (500 ms). Selecting a preset fills the duration field; press **START CUSTOM EXPOSURE** to run it.

The duration is a firmware command interval measured from when the servo-open command is issued. It includes servo travel time and is **not** a guaranteed optical exposure duration. A hobby servo is not a precision shutter or laser safety interlock.

### Wi-Fi version wiring and safe commissioning

| SG90 lead | Connection |
|---|---|
| Signal (usually orange/yellow) | Nano ESP32 D9 |
| +5 V (usually red) | External regulated +5 V servo supply |
| GND (usually brown/black) | External supply GND and Nano ESP32 GND |

Power the Nano ESP32 separately through USB-C or its supported input. Do not power the servo from the board's 3.3 V pin. The Nano ESP32 does not provide a conventional 5 V output pin; use a separate regulated 5 V servo supply and connect its ground to board GND.

Test all motion with the laser disabled. Verify that the mechanism does not bind or stall. Use an independent, secure beam stop / enclosure; never rely on the SG90 as the only laser barrier.

## Serial protocol

The browser sends one JSON object per line and firmware replies with one JSON object per line.

Commands:
- `{"cmd":"OPEN"}`
- `{"cmd":"CLOSE"}`
- `{"cmd":"EXPOSE","ms":1000}`
- `{"cmd":"CANCEL"}`
- `{"cmd":"SET","open":68,"closed":0,"relief":4,"travelMs":150}`
- `{"cmd":"STATUS"}`

The `travelMs` setting is a conservative software allowance for servo movement, not a measured or closed-loop travel-time measurement. Adjust angles for the specific mechanism and verify that the servo does not bind or stall.

## Safe commissioning

1. Disconnect or disable the laser. Test the shutter with no beam.
2. Verify the mechanical closed position and ensure the mechanism moves freely by hand (with power off).
3. Connect servo power and USB; confirm the shutter moves to the closed angle at startup.
4. Test OPEN and CLOSE several times. Watch for binding, buzzing, overheating or brownouts.
5. Start with long exposures and measure actual optical transmission timing using a suitable photodiode and oscilloscope.
6. Never depend on this servo mechanism as the sole laser safety barrier. Use a separate, secure beam stop / enclosure and follow appropriate 530 nm laser safety practices. A stalled, disconnected or unpowered servo may leave the shutter in an unsafe state.

## Original project credit and license

The mechanical design and original Arduino Servo-Shutter project are by **hackaninstant**:

- Original repository: https://github.com/hackaninstant/Servo-Shutter
- Original Thingiverse design: https://www.thingiverse.com/thing:6033871

This repository is an adaptation, not the original design. The original repository is licensed under **GNU General Public License v3.0**. See [LICENSE](LICENSE) for the license text. Retain upstream attribution and license notices when redistributing applicable original code or design files. Review the upstream license and any separate Thingiverse model terms before redistributing STL files.

## Project layout

- `firmware/NanoServoShutter/` — Arduino Nano firmware
- `web/` — browser-based USB serial control panel
- `mechanical/` — location for the original STL archive / extracted STL files (not included in this initial code commit; obtain them from the credited upstream project)

## Disclaimer

This is an experimental hobby project. It is not a certified photographic shutter, laser interlock, or safety-rated device.
