# SC1000 app: tempo, clock and beat repeat (built)

How the tempo and beat-repeat features ended up, and why. The current controls are in `Applications/SC1000/README.md`.

## What was built
- **Internal clock only, no MIDI clock out.** The first plan sent 24 PPQN clock on USB. Nothing listens to it: the SC1000 drops clock bytes (rasteri `dicer.c` only parses 0x80/0x90/0xB0/0xE0). So the app keeps a clock purely for its own timing:
  - `BeatUs() = 60,000,000 / bpm`.
  - Beats are measured from `clockOriginUs`.
  - `SetBpm()` moves the origin so the current beat phase is kept when the tempo changes.
- **BPM page** (Settings → orange pad) replaced the planned "Tempo page" with start/stop. Reused pieces:
  - `UI4pxNumber`.
  - `NoteBPMTapper`, included from `../Note`.
  - `UINumberModifier` with {−25, −10, −5, −1, 1, 5, 10, 25}, range 20–299.
  - A beat light on the bottom row.
  - The page's loop runs `Background()`, so repeats and queued messages keep going while it's open.
- **Not reused:** the Note app's `MidiClock`. Its `Tick()` resets `last_tick` to "now" on every tick, so lateness adds up and it runs slow. SC1000 schedules from the previous target time instead (drift-free).

## Beat repeat
Hold a pad to capture the deck's position into scratch cue 65 and jump back to it at a fixed rate. Release to stop.

- **Rates:** 4 pads per deck (row 3) for ¼, ⅛, 1/16 and 1/32 notes. Interval = `BeatUs() * 4 / rate`. One engine per deck, so both decks can repeat at once.
- **Start:**
  - Immediate by default. Cue 65 is set on press, and repeats follow every interval *from the press*, not from the clock's grid. The user's reasoning: the SC1000 is played by ear against free-running tracks, so an accurate length matters more than grid alignment.
  - With **BR_Q** on (Settings), the start waits for the next beat of the internal clock instead.
- **No delete before setting.** Deleting first would add ~15 ms of latency. The risk accepted: if cue 65 already exists (a crash mid-hold, or a `.cue` file defining it), the first press jumps to that old point.
- **Several pads held:** the newest rate takes over from the last trigger. Releasing it falls back to the next-newest held pad.
- **Latch:** SHIFT + a rate pad latches it.
  - The latch ends when that pad is pressed again or another rate on the same deck is pressed.
  - The new rate takes over before the old latch is dropped, so the captured position is kept.
  - A latch toggle's own release is ignored.
- **Stop:**
  - When no pad is held or latched, send shift on (ch 5 note 127), cue 65, then shift off, deleting the scratch cue.
  - A quantized start that hadn't fired yet is cancelled silently.
  - `End()` also deletes any running repeat.
- **Releases on Settings:** a pad released after switching to Settings still stops its repeat, so it can't get stuck.
- **Pad pulse:** each rate pad fades from bright to dim over its interval, following the internal clock. Held or latched pads show full brightness.
- **Verified with aseqdump:** at 120 BPM, a ⅛ repeat fires every 250 ms. Latching, switching rate and stopping all behave as described.

## Known limits
- **Not phase-aligned with the track.** The internal clock isn't phase-aligned with whatever the SC1000 is playing. The pulse and BR_Q beats won't land on the track's downbeat.
- **Shift is shared.** The cue delete uses the SC1000's global shift, which it shares with the hardware shift button.
- **Stick writes.** Each set and delete rewrites the track's `.cue` file on the SC1000's USB stick. Listen for glitches when testing on the real unit.

## Ideas not built
- **Downbeat sync pad:** tap it on the track's "1" to re-anchor `clockOriginUs`, so the pulse and BR_Q line up with the music.
- **Phase 2 cut sequencer** from the build plan, which would run on this clock.
