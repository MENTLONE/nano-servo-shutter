# iPhone Wi-Fi Setup — ESP32 Servo Shutter

This is the Wi-Fi version of the Nano Servo Shutter project. It uses an ESP32 DevKit / ESP32-WROOM-32 in place of the Arduino Nano. The ESP32 hosts the control page itself, so no computer, router, internet connection, or phone app is required during operation.

## Hardware

- ESP32 DevKit board with classic ESP32-WROOM-32 module
- SG90 9g servo
- Regulated external 5V supply rated for at least 1A (2A is a comfortable choice)
- Jumper wires
- USB cable for programming the ESP32

## Wiring

| SG90 lead | Connect to |
|---|---|
| Brown / black (GND) | External 5V supply negative / GND |
| Red (+5V) | External regulated +5V |
| Orange / yellow (signal) | ESP32 GPIO18 |

Connect the external supply GND to an ESP32 GND pin. This common ground is essential. Do not power the SG90 from the ESP32 3V3 pin. Avoid powering the servo from the ESP32 board's 5V pin; use the separate regulated supply. The ESP32 itself is powered through USB while programming and testing.

## Arduino IDE setup

1. Install Arduino IDE.
2. In Boards Manager, install **esp32 by Espressif Systems**.
3. In Library Manager, install **ESP32Servo** by Kevin Harrington.
4. Open `firmware/ESP32WiFiShutter/ESP32WiFiShutter.ino`.
5. Select your ESP32 board. For a typical ESP32-WROOM-32 DevKit, choose **ESP32 Dev Module**.
6. Select the serial port and upload.
7. Open Serial Monitor at 115200 baud if you want to confirm startup. It prints the access-point name and IP address.

## Connect from iPhone

1. Power the ESP32.
2. On iPhone, open **Settings → Wi-Fi**.
3. Join **HoloShutter-ESP32**.
4. Enter the default password **HoloShutter530**.
5. If iOS says the network has no internet, choose **Stay Connected** / continue using the network.
6. Open Safari and enter **http://192.168.4.1**.
7. Add the page to the Home Screen if desired: Safari Share → Add to Home Screen.

Change the default Wi-Fi password in the firmware before using the controller in a shared or public setting. Use a password of at least 8 characters.

## Controls

- Open: command the servo to the configured open angle.
- Close: command the servo to the configured closed angle.
- Cancel / Close: stop a timed exposure and close the shutter.
- Timed exposure: enter a duration in milliseconds (1–3,600,000).
- Servo calibration: adjust open and closed angles from the phone. These settings are held in RAM and reset to firmware defaults after power cycling.

## Important timing and laser safety

The timed exposure clock begins when the ESP32 sends the servo-open command. It does not wait for the SG90 to physically reach the open position. The actual optical transmission time is therefore different from the requested duration and must be measured/calibrated (for example with a photodiode and oscilloscope).

The SG90 is not a precision optical shutter and is not a laser safety interlock. A servo can stall, lose power, or fail mechanically. Test motion with the laser disabled and use an independent secure beam stop/enclosure. Never rely on this shutter as the sole laser barrier.

## Troubleshooting

- **Wi-Fi network not visible:** check ESP32 power and upload; reset the board.
- **iPhone says no internet:** stay connected to the ESP32 network; that is expected because the ESP32 is providing a local-only network.
- **Page will not load:** verify iPhone is still joined to HoloShutter-ESP32 and enter the full URL `http://192.168.4.1` (not https).
- **Servo jitters or ESP32 resets:** use a separate regulated 5V servo supply with adequate current and connect grounds together.
- **Servo moves in the wrong direction or hits its stops:** keep the laser disabled and adjust the open/closed angles gradually.
