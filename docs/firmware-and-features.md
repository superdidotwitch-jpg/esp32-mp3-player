# Firmware and features

This describes what the finished unit does and how its behavior is organized, a spec to build from, not a pasted source file (every builder's exact pin choices and library versions will differ enough that a literal code dump would need rewriting anyway).

## Core interaction model (single-encoder unit)

- Turn the encoder: changes whatever the current mode is pointed at (volume, track, or an EQ/shuffle setting)
- Short click: play/pause (resumes mid-track rather than restarting)
- Long-press (~0.6s): cycles to the next mode (e.g. Volume → Track → EQ → Shuffle → back to Volume)
- After a few seconds without input, the screen switches to an idle/screensaver state and the encoder's turn action returns to controlling volume by default
- Shuffle state is remembered across power cycles; with shuffle off, tracks play in file order and wrap back to the first track after the last

## Two-encoder variant

Splits the above across two encoders instead of cycling modes on one: encoder 1 turn = track, click = play/pause; encoder 2 turn = volume, click = cycles EQ presets (saved across power cycles). Optional extra slide switches can add a lock (disables both encoders) and a shuffle toggle.

## Startup sequence

Worth building in from the start rather than adding later: the DFPlayer module can take a moment to initialize after power-on, and if the main controller tries to talk to it immediately, it may read as absent even though it's fine. A short fixed delay after power-up, followed by retrying the DFPlayer handshake for several seconds (with an on-screen "Starting..." / "Looking for card..." sequence) avoids needing a manual reset button press every time the unit is switched on.

## Optional extras (all built and working on at least one unit)

- **OTA (over-the-air) firmware updates**, once the first upload is done over USB, later firmware updates can be pushed over WiFi instead of needing a cable every time.
- **NTP time sync**, connects to WiFi briefly at startup to set a real clock, then keeps running on its own even after leaving that network (it just won't re-sync until it's back in range and power-cycled).
- **A simple local web dashboard**, the controller hosts a small page on its own local-network address showing the currently playing track, volume, and play state, auto-refreshing. Handy for checking status without looking at the unit itself.
- **Playback resume**, current track, volume and EQ setting are written to flash so they survive a power cycle instead of resetting to defaults every time.
- **An idle-screen animation**, not functionally necessary, but a nice touch: branding or a short animation after a few seconds of no input, replaced instantly on the next knob turn or click.

## Features considered and deliberately dropped

- **Bluetooth audio output**, looked like a simple software addition at first, but isn't: if your build uses a DFPlayer-style module, *it* decodes the MP3 and sends analog audio straight to the headphone jack, the main controller never actually receives the decoded audio data, only play/pause/volume commands. Real Bluetooth output would mean the main controller decoding and streaming audio itself (e.g. via I2S), which is a much bigger redesign, not a quick add-on.
- **Built-in microphone/voice recording**, considered, then dropped for this build; if pursued, better suited to a from-scratch unit than retrofitted onto a finished one.

## WiFi credentials

Keep these out of source control. Use a separate, untracked config file (or your IDE/build system's standard secrets mechanism) for SSID and password rather than hardcoding them in a file that gets committed.
