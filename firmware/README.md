# Firmware

The real code running on the finished unit.

| Sketch | For | Controls |
|---|---|---|
| [spider/spider.ino](spider/spider.ino) | The two-knob unit ("Spider") | Two rotary encoders, a lock switch and a shuffle switch |

The one-knob version used on the second unit is described in [docs/firmware-and-features.md](../docs/firmware-and-features.md). Its source was not kept, so it is not here yet.

## Before uploading

**Arduino IDE settings**

- Board: **ESP32 Dev Module**
- Flash Mode: **DIO**. On these boards the default setting leaves the sketch never starting, with only boot text in the Serial Monitor. See [docs/lessons-learned.md](../docs/lessons-learned.md).

**Libraries to install** (Library Manager)

- Adafruit GFX Library
- Adafruit SSD1306
- DFRobotDFPlayerMini

Everything else it uses comes with the ESP32 board package.

**WiFi**

Near the top of the sketch, replace `YOUR_WIFI_NAME` and `YOUR_WIFI_PASSWORD` with your own. WiFi is optional. Without it the player works the same, with no clock, no web page and no wireless updates.

## Pins

| Part | Pin on the part | ESP32 pin |
|---|---|---|
| OLED | SDA / SCL | 21 / 22 |
| DFPlayer Mini | TX / RX | 16 / 17 (the two are crossed: player TX to ESP32 RX2, player RX to ESP32 TX2) |
| Knob 1 (track) | CLK / DT / SW | 32 / 33 / 25 |
| Knob 2 (volume) | CLK / DT / SW | 34 / 27 / 26 |
| Lock switch | outer pin | 13 (centre pin to ground) |
| Shuffle switch | outer pin | 14 (centre pin to ground) |

## What it does

- Remembers track, volume, EQ and play state across power cycles
- Waits for the DFPlayer at power-up and retries, so there is no reset button to press
- Looks for home WiFi for 4 seconds at start. If found: sets the clock, serves a small status page, and accepts wireless updates under the name `spider-mp3`. If not found, or after 30 seconds away from home, it switches WiFi off to save battery
- After 5 seconds without input, shows a screensaver: a name card, then a flying-kick figure sliding across. The first touch only wakes the screen

## Making it yours

The screensaver text is in `showName()` and the picture is the `kickPic` array, a 60 by 46 pixel one-colour bitmap. Swap in your own.
