# Breakbeat app for the Mystrix Pro

A grid controller for the breakbeat track of **slompler**, the Raspberry Pi sampler in `~/code/slompler`. The Mystrix sends notes and CCs, and the sampler sends back pad LED states and its current settings. Both sides follow `docs/midi-spec.md` in the slompler repo.

- Plug the Mystrix into the Pi (or the computer running slompler) over USB.
- The sampler connects to it automatically and sends LED feedback only to MIDI ports with "Mystrix" in the name. Change this with `feedback.ports` in `midi_map.toml`.
- Everything is on MIDI channel 1.

Rows and columns below are 1-based, counted from the top-left.

## Play page

The whole 8×8 grid is the loop's slices. Pad n (left→right, top→bottom) sends note 36 + n.

| Pad colour | Meaning |
|---|---|
| Dim blue | A slice. Brighter on each beat, brightest on each bar |
| Orange | Master playhead |
| Pink | Held slice, playing |
| Yellow | Pressed, waiting for the next slice boundary (quantize) |
| Off | No slice here (shorter loops or coarser slicing) |
| All dim white | No feedback from the sampler yet. The pads still work |

- Press = jump to the slice; hold = repeat it; release = back to the playhead.
- While the sampler is stopped, holding a pad auditions its slice (lit as held); no playhead is lit. Start it from the Settings page (row 6), the touchscreen, MIDI Start (e.g. the Elektron's play button) or note 34.
- **Function key:** tap to open Settings, hold to exit the app.

## Side touch strips (both pages)

Punch-in slice FX for held slices, active only while you touch a strip (left or right, same on both):

- **Upper half:** reverse.
- **Lower half:** ping-pong, starting with a backward pass.
- Let go and the HELD SLICE FX setting applies again; the setting itself never changes.
- With QUANTIZE on, punching in and out waits for the next slice boundary.
- It only changes held slices (and auditions while stopped); the free-running loop is unaffected.
- The app turns the strips on while it's open, even if they're off in the device settings. Sends CC 28 (0 released, 64 reverse, 127 ping-pong).

## Settings page (tap the function key)

| Row | Pads |
|---|---|
| 1 | SLICES ¼ · ⅛ · 1/16 (cols 1–3, cyan) · BARS 1–4 (cols 5–8, blue) |
| 2 | PLAYHEAD fwd · rev · random (cols 1–3, green) · HELD SLICE FX off · reverse · ping-pong (cols 6–8, purple) |
| 3 | FOLLOW REAL TIME (col 1) · QUANTIZE (col 2), orange toggles · PANIC (col 8, red) |
| 4 | DJ FILTER: cols 1–3 low-pass strong→light, cols 4–5 off (white), cols 6–8 high-pass light→strong |
| 5 | BIT CRUSH: col 1 off, cols 2–8 more and more crush (lit as a bar) |
| 6 | PLAY / STOP (col 1, green): bright while playing. Play starts from the top of the loop |
| 7 | REC MODE clock · manual (cols 1–2, red) · REC BARS 1–4 for clock mode (cols 5–8, blue) |
| 8 | REC (col 1) · UNDO (col 3, green) · PREV / NEXT loop (cols 6–7, blue) |

- The selected option is bright and the others are dim. The pads show the sampler's real state, so changes made on the touchscreen show up here too.
- **REC** (one pad):
  - Idle: press to arm (clock mode) or start (manual mode).
  - Otherwise: press to stop or cancel.
  - Colour:
    - dim red = idle
    - blinking yellow = armed, waiting for MIDI Start
    - blinking red = recording
    - white = saving
- **Switching pages** releases any held slices, so nothing gets stuck.
- **Tap the function key** to go back to Play.

## MIDI

| Sent | Message (channel 1) |
|---|---|
| Slice pads | Note on / off, notes 36–99 |
| Settings | CC 20–27, 74, 75 with the values in the spec |
| Actions | CC 80–85 (value 127) |
| Play / stop | CC 88: 127 play from the top, 0 stop |
| Refresh request | CC 86, sent on launch and every 2 s until the sampler answers |
| Keep-alive | CC 87 = 127 every second while the app is open; 0 on exit |

| Received | Used for |
|---|---|
| Note on 36–99, velocity 0–4 | Pad LED state (0 off, 1 slice, 2 playhead, 3 held, 4 queued) |
| CC 20–27, 74, 75 | Settings mirror |
| CC 88 | Playing or stopped |
| CC 90 | Record state |

- Sent on the USB port only.
- **Why the keep-alive:** the sampler sends feedback only while the app is open. A Mystrix sitting in another app or the launcher doesn't read its MIDI input, and feedback piling up there jammed its USB port (and the Pi's USB hub, which the audio interface shares).
- The sampler stops feedback 3 s after the last keep-alive. If feedback stops for 5 s, the app goes back to the dim "offline" look and asks for a refresh again.
- On exit the app releases any held slices.

## Code

- `Breakbeat.h`: MIDI contract constants (they must match the spec) and app state.
- `Breakbeat.cpp`: MIDI in and out, both pages, and LEDs.
