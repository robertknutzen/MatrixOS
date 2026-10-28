# SC1000 app for the Mystrix Pro

A one-way USB MIDI controller for the Portablism SC1000/SC500. It targets the default "Lemur preset" MIDI map of rasteri/SC1000 v1.7.

- Plug the Mystrix into the SC1000's USB host port before powering the SC1000 on. The SC1000 only scans for MIDI devices at boot.
- The SC1000 never sends anything back, so the app keeps its own record of which cues are set and where the pitch is.
- Status: tested and working on a real SC1000.

Rows and columns below are 1-based, counted from the top-left.

## Main page

| Row | Columns 1–4 (samples deck, blue) | Columns 5–8 (beats deck, green) |
|---|---|---|
| 1 | Cues S0–S3 | Cues B0–B3 |
| 2 | Cues S4–S7 | Cues B4–B7 |
| 3 | Beat repeat ¼ · ⅛ · 1/16 · 1/32 | Beat repeat ¼ · ⅛ · 1/16 · 1/32 |
| 4 | — | — |
| 5 | PREV (col 1), NEXT (col 3) | PREV (col 6), NEXT (col 8) |
| 6 | Pitch − · RESET · + (cols 1–3) | Pitch − · RESET · + (cols 6–8) |
| 7 | — | — |
| 8 | SHIFT (col 1) | PERFORM, purple (col 6) · SHIFT (col 8) |

- **Cues:** the first press sets the cue and later presses jump to it. Set cues are brighter. While SHIFT is held, the cues turn red and a press deletes that cue.
- **Beat repeat:**
  - Hold a pad to capture the current position and jump back to it at that rate. Release to stop.
  - Holding several rate pads uses the newest one.
  - **SHIFT + a rate pad latches it.** It keeps repeating until you press that pad again or another rate on the same deck.
  - The pads pulse at their rate in time with the app's tempo.
  - The repeat starts the moment you press, unless BR_Q is on (see Settings).
- **PREV/NEXT** change file. With SHIFT held they change folder instead (the pads brighten to show it).
- **Pitch:**
  - − and + nudge by 0.1%, and repeat when held.
  - Samples RESET puts the pitch back to 0% and the key back to original. Beats RESET puts the pitch back to 0%.
  - The brighter pad of each trio shows whether the deck plays slower, normal or faster.
- **Function key:** tap to open Settings, hold to exit the app.

## Settings page (tap the function key on Main)

- **BACK** (green, top-left) or a function key tap returns to Main.
- **Exit** (red X in the top-right 4×4) leaves the app.
- **SCALE** (yellow, row 8 col 1) opens the scale page:
  - The piano at the top picks the key.
  - The grid at the bottom picks one of 16 scales: Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Harmonic Minor, Melodic Minor, Blues, Major Pentatonic, Minor Pentatonic, Hungarian Minor, Whole Tone, Major Blues, Chromatic.
- **BR_Q** (purple, row 8 col 3): beat-repeat quantize. When it's on, a repeat waits for the next beat of the app's tempo before starting.
- **BPM** (orange, row 8 col 8):
  - Tap the number to tap the tempo.
  - The row below steps it by −25 / −10 / −5 / −1 / +1 / +5 / +10 / +25 (range 20–299).
  - The bottom row flashes on the beat.

## Perform page (PERFORM pad on Main)

- **Rows 1–3** work exactly as on Main.
- **Row 4:** SHIFT at cols 1 and 8.
- **Row 5, col 1:** ORIGINAL PITCH (red). Plays the sample at its original pitch.
- **Rows 6–8:** keys that repitch the samples deck, only showing notes in the chosen scale.
  - Each row is one octave. The middle row starts on the key root nearest the sample's original pitch, the top row is an octave up, the bottom row an octave down.
  - Root notes are brighter, and the note playing now is full white.
  - Notes more than 2 octaves from the original are dark.
  - Pressing a key first resets the samples pitch nudge to 0%, so the tuning holds. The note stays after you let go.
  - This assumes **the sample is tuned to C**.
- **Function key:** tap to go back to Main.

## MIDI sent

| Action | Message (channels 1-based) |
|---|---|
| Samples cue n / beats cue n | Note n on ch 2 / ch 1 |
| Delete a cue | Ch 5 note 127 (shift on), the cue note, ch 5 note 127 off |
| Beat repeat | Note 65 (a scratch cue) on the deck's channel; deleted on release |
| Pitch | Pitch bend on ch 1 / ch 2 (assumes the SC1000's `pitchrange` is 50%) |
| Keys | Note on ch 4 (60 = original pitch) |
| File / folder | Ch 5 notes 2–9 |

- Note-on only: no note-off, aftertouch or MIDI clock.
- Sent on USB port 1 only, with at least 5 ms between messages, because the SC1000 holds just one pending MIDI command.
- Saved between sessions: BPM, key, scale and BR_Q.

## Known limits

- **Tempo isn't locked to the track.** The app's tempo is its own clock, and the SC1000 has no clock to follow. Repeat lengths are accurate, but the beat-repeat pulse and BR_Q beats won't line up with the track's downbeat.
- **Shift is shared.** Deleting cues uses the SC1000's global shift, which it shares with its hardware shift button.
- **No "restart track" action** exists in the SC1000's MIDI map.

## Code

- `SC1000.cpp`: core, clock, beat repeat, pitch, output queue.
- `SC1000Pages.cpp`: input and LEDs for the three pages.
- `SC1000Menus.cpp`: BPM and scale pages.
- Reuses the Note app's `BPMTapper.h`, `Scales.h` and `ScaleVisualizer.h` through an include path.

Design history is in the repo root:
- `SC1000 Controller app for Mystrix Pro — build plan.md`
- `SC1000 tempo + clock notes.md`
- `SC1000 scale notes.md`
