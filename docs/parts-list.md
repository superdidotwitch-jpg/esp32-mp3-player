# Parts list

## Per unit

| Part | Role | Notes |
|---|---|---|
| ESP32 DevKit (USB-C) | Main controller | Any common DevKit-style board works; confirm its onboard 3.3V/5V pinout matches whatever wiring guide you follow |
| DFPlayer Mini | MP3 decoding + microSD slot + built-in headphone-level output | No separate amplifier needed — it drives a 3.5mm jack directly |
| 0.96" OLED, SSD1306, I2C, 128x64 | Status display | I2C keeps it to 2 signal pins (SDA/SCL) |
| KY-040 rotary encoder (with push-button) | Primary input | One per unit minimum; the finished unit uses two — see [wiring.md](wiring.md) |
| 3.7V LiPo battery, PH2.0/JST-PHR-02 connector | Power | **Verify the connector pitch before buying** — see the gotcha below |
| TP4056 charging module, Type-C, PH2.0 | Battery charging | Only charges the battery — does not boost voltage. See the boost converter note below |
| Boost converter module, ~0.9-4.2V in to 5V out | Powers the ESP32 from the raw LiPo cell | **Not optional** — without it, the ESP32's onboard regulator is starved on battery power even though the power LED stays lit. See [lessons-learned.md](lessons-learned.md) |
| Mini breadboard (170-point) + jumper wires | Prototyping platform | Two side-by-side mini breadboards, with the ESP32 straddling the seam, gives more room than one board alone |
| 3.5mm headphone jack breakout, screw-terminal | Audio out | Screw terminals avoid needing to solder directly to the DFPlayer's tiny DAC pins |
| microSD card | Song storage | Any capacity the DFPlayer supports; format and load with MP3 files before first real playback test |
| Small speaker (8 ohm, ~1W) *(optional)* | Secondary audio output | Wired straight to the DFPlayer's built-in speaker amp pins, separate from the headphone jack circuit |
| Slide switch(es) *(optional)* | Power switch, and any extra mode switches (lock, shuffle) you want | Wire inline on the positive line between the boost converter's output and the ESP32's power input — not on the ground line, and not between the battery and the charger |

## The connector-pitch gotcha

Cheap LiPo batteries and charging boards both often say "JST" on the listing, but several incompatible pitch sizes get sold under that same generic name. Before ordering a battery and a charger from different listings, confirm both specify the *same* connector code (e.g. PH2.0 / JST-PHR-02) — this was flagged as the single easiest thing to get wrong in the whole parts list.

## Sourcing notes

- At 3+ units, a general marketplace (Amazon-style multi-packs) worked out noticeably cheaper per unit than a specialist electronics retailer, and left useful spares.
- A specialist retailer was still the better call for anything needed in just one or two units, or for a part a generic multi-pack doesn't include at all (the screw-terminal jack breakout and the boost converter, in this build).
- A bare SMD-only microcontroller module (no header pins) will not work for a no-soldering, breadboard-based build — confirm any ESP32 listing explicitly includes pin headers.
- Approximate per-unit build cost across all three units, parts only (excluding tools): in the neighborhood of €35-40, assuming multi-pack pricing and no wasted/broken parts. Budget extra — at least one spare of the cheap, fragile boost converter module is worth having; its bare pads are very easy to damage while wiring.

## Tools used

A basic small-tip screwdriver set (for the jack's screw terminals) and a multimeter (essential — see [lessons-learned.md](lessons-learned.md) for how many real problems it diagnosed). No soldering iron was required for the build described here, though a soldering setup was picked up afterward for other projects.
