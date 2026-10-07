# Firmware

The real code running on the two finished units.

| Sketch | For | Controls |
|---|---|---|
| [spider/spider.ino](spider/spider.ino) | The two-knob unit ("Spider") | Two rotary encoders, a lock switch and a shuffle switch |
| [octopus/octopus.ino](octopus/octopus.ino) | The one-knob unit ("Octopus") | One rotary encoder: turn to change, short click to play or pause, hold to move between volume, track, EQ and shuffle |

Both use the same libraries, the same settings and the same OLED and DFPlayer pins. Octopus only uses Knob 1 from the pin table and has no switches.

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

## What Spider does

- Remembers track, volume, EQ and play state across power cycles
- Waits for the DFPlayer at power-up and retries, so there is no reset button to press
- Looks for home WiFi for 4 seconds at start. If found: sets the clock, serves a small status page, and accepts wireless updates under the name `spider-mp3`. If not found, or after 30 seconds away from home, it switches WiFi off to save battery
- After 5 seconds without input, shows a screensaver: a name card, then a flying-kick figure sliding across. The first touch only wakes the screen

## What Octopus does differently

- One knob does everything. A `>` on the screen marks what turning the knob changes right now. Holding the knob for 0.6 seconds moves to the next one: volume, track, EQ, shuffle
- Shuffle is a menu item instead of a switch, and it is remembered across power cycles
- Its screensaver types out a short text, then shows a guitar with notes flying off it. The text is `ssText` near the top of the sketch
- Wireless updates use the name `octopus-mp3`

## Making it yours

In Spider, the screensaver text is in `showName()` and the picture is the `kickPic` array, a 60 by 46 pixel one-colour bitmap. Swap in your own.
