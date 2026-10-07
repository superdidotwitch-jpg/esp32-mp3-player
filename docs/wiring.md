# Wiring

## OLED (I2C)

Standard I2C wiring: SDA and SCL to the ESP32's I2C-capable pins (commonly GPIO21/GPIO22 on a typical DevKit, confirm against your specific board's pinout diagram), plus VCC and GND off a shared power rail.

## Rotary encoder(s)

Each KY-040 needs three digital pins: CLK, DT, and SW (the built-in push-button), plus VCC/GND. The finished unit in this build uses two encoders, one for track/play-pause, a second for volume/mode-cycling, each with its own three signal pins, sharing the same VCC/GND rail as the OLED.

**Gotcha:** if you wire two encoders, give each one fully separate CLK/DT/SW rows on the breadboard. Letting their signal pins share a row (even if VCC/GND are meant to be shared) makes both encoders read as the same one.

Note: not every GPIO pin has an internal pull-up resistor. If a pin you've chosen for an encoder signal doesn't, check whether your specific encoder module has its own onboard pull-up (most do) before assuming the wiring is faulty.

## DFPlayer Mini (UART)

Three pins to the ESP32: VCC, GND, and a crossed RX/TX pair, the DFPlayer's RX goes to one of the ESP32's TX pins, and the DFPlayer's TX goes to one of the ESP32's RX pins (crossed, not straight across). At typical ESP32 logic levels, no voltage-divider resistor is needed on this connection.

**Reading the DFPlayer Mini's pin labels:** look for a small white dot/circle silkscreened near one corner of the board, that marks pin 1 (VCC). Counting down that physical column from the dot: 1 VCC, 2 RX, 3 TX, 4 DAC_R, 5 DAC_L, 6 SPK_1, 7 GND, 8 SPK_2. The numbering wraps around the board like a DIP chip rather than restarting on the opposite edge, easy to misread if you assume it restarts.

**A "DFPlayer not detected" error with everything wired correctly** is often just a missing or unformatted microSD card, not a wiring fault, the module frequently won't acknowledge the handshake with no card present.

## Audio output

From the DFPlayer: pin 5 (DAC_L) to the jack breakout's L terminal, pin 4 (DAC_R) to R, pin 7 (GND) to GND. A 4-terminal breakout board's 4th terminal (often labeled V, for composite video) is unused here.

For an optional second speaker output: the DFPlayer's own speaker amp pins (6 and 8 in the numbering above) wire straight to a small 8-ohm speaker, with neither pin going to ground.

## Power chain, read this before connecting a battery

```
LiPo battery → TP4056 charger → boost converter → (switch) → ESP32 power input
```

- The TP4056 **only charges the battery**, it does not raise voltage. Feeding its output straight into the ESP32 without a boost converter leaves the board under-powered on battery (even though the charging LED looks completely normal), causing the screen/encoders/DFPlayer to misbehave intermittently. The boost converter is not optional for reliable battery operation.
- **Known batch-wide gotcha:** on the cheap TP4056 boards used in this build, the "B+" and "B-" pad labels were confirmed, across every unit in the batch, to be printed backwards from their actual polarity. Verify with a multimeter before trusting the silkscreen: connect the battery, measure across the two pads, and whichever reads positive is your real B+, regardless of what's printed. Don't assume a single board's reversed labels are a one-off defect, if one board in a batch is wrong, check whether they all are.
- **Before connecting any new battery/charger pair to a fresh board**, check polarity with a multimeter first: red probe on the wire you intend for the power-positive input, black on ground-intended. A reading of roughly +3.6 to +4.2V confirms correct polarity; a negative reading means swap the two wires before connecting to the ESP32.
- If you add a power switch, wire it inline on the *positive* line between the boost converter's output and the ESP32's power input pin, not between the battery and the charger, and not on the ground line. This way the battery still charges normally over USB regardless of switch position; the switch only cuts power to the ESP32 itself.
- **Never have USB and the battery connected to the board at the same time.** Doing so can corrupt the flash read at boot, producing a repeating checksum-error reset loop that looks exactly like hardware damage but isn't, fully disconnect one power source before connecting the other.

## Build-order recommendation

Wire and test one component at a time rather than assembling everything before the first test: bare ESP32 alone (confirm a simple serial print works) → add the OLED only, test → add the encoder(s), test → add the DFPlayer wiring (no new code yet), confirm the existing sketch still runs without a brownout → then add DFPlayer code. This catches wiring mistakes and power issues (see [lessons-learned.md](lessons-learned.md)) at the specific step that caused them, instead of debugging a fully-wired board with several possible culprits at once.
