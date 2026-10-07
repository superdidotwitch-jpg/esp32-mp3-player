# Lessons learned

Real problems hit during this build, in rough order of how confusing they were to diagnose.

## "Everything got very hot", a shorted boost converter

**Symptom:** immediately after wiring the charger → boost converter → switch → battery chain and connecting power, components got hot fast.

**Diagnosis:** a multimeter traced it to a wiring mix-up, the boost converter's output had been connected back to what was assumed to be ground, while its actual ground pad had been routed to the switch, which effectively shorted the converter's output straight to ground.

**Lesson:** when a power chain gets warm immediately on connection, disconnect power first, then verify every connection in that chain against a clear written-down plan before reconnecting, don't try to trace it live with power still applied.

## The reversed TP4056 polarity labels

**Symptom:** a charger board read negative voltage on the pad its own silkscreen labeled "B+".

**Diagnosis:** confirmed with a multimeter, then confirmed again on a second, then a third board from the same batch, every TP4056 module in that batch had its B+/B- pads printed backwards from their actual polarity.

**Lesson:** don't treat one mislabeled board as a fluke and move on, if a cheap module's labels are wrong once, check whether the whole batch shares the defect before wiring the next unit the "normal" way and getting the same shock again.

## Boards that looked bricked but weren't

**Symptom:** after wiring was finished, a board would only ever print ROM bootloader text in the serial monitor and never run the actual uploaded program, looked exactly like a board damaged during the earlier overheating incident.

**Diagnosis:** unrelated to any hardware damage. The IDE's flash-mode setting (commonly defaulting to "QIO") didn't match what these specific clone boards needed ("DIO"). Switching that one setting and re-uploading fixed every affected board instantly.

**Lesson:** "stuck at bootloader, never runs my code" on a clone/budget board is worth checking against the IDE's flash mode setting before assuming the chip itself is damaged.

## Brownout resets when everything was wired at once

**Symptom:** running the screen, encoder, and MP3 decoder module together off USB power intermittently triggered a brownout reset.

**Diagnosis:** combined current draw from several peripherals plus the controller's own power-up spike exceeded what the power source could comfortably supply in that moment.

**Lesson:** this is exactly why building and testing one component at a time (see [wiring.md](wiring.md)) matters, it turns "the fully-wired board randomly resets" into "it broke the moment I added the third peripheral," which is a much smaller problem to solve.

## USB and battery fighting each other

**Symptom:** after reconnecting a battery, a board entered a reset loop showing a flash checksum error, with a different checksum reported on each reset, looked like corrupted or damaged flash memory.

**Diagnosis:** ruled out via a blank test sketch with no libraries, which failed identically. The actual cause was having USB and the battery connected to the same board simultaneously, which corrupts the flash read at boot. Removing the battery and leaving only USB connected fixed it immediately.

**Lesson:** never power a board from two sources at once. Fully disconnect one before connecting the other, every time, not just "usually."

## The silent under-powering on battery

**Symptom:** the screen and controls behaved oddly on battery power even though the charging circuit's LED looked completely normal.

**Diagnosis:** measuring the controller's own 3.3V output pin on battery power showed a significantly lower voltage than its rated output. The charger module only charges the battery, it doesn't boost the raw LiPo cell's voltage, so the controller's onboard regulator was being asked to run with less headroom than it needed, starving every peripheral fed from that same regulator at once.

**Lesson:** a charging module's working LED only tells you charging is happening, not that downstream power is adequate. If a battery-powered device misbehaves while a USB-powered one doesn't, measure the actual voltage reaching the controller before assuming it's a software or wiring bug.

## General takeaways

- **A multimeter is not optional for this kind of build.** Every diagnosis above started with one. If you only buy one piece of test equipment before starting something like this, make it that.
- **Build and test incrementally, not all-at-once.** Every brownout- and wiring-related bug here became dramatically easier to isolate once testing happened after each component was added, rather than after the whole thing was wired.
- **A problem that looks like hardware damage often isn't.** Three separate issues in this build (the bootloader-only boards, the checksum-error reset loop, and the erratic battery behavior) all initially looked like a dead or damaged board, and none of them were.
- **Use short wires from the start.** Long jumper wires work fine electrically but make later debugging and enclosure-fitting harder than it needs to be; worth the extra few minutes up front to cut or choose appropriately short ones.
