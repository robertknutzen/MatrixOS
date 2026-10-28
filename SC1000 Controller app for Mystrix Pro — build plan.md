# SC1000 Controller app for Mystrix Pro — build plan

<https://github.com/203-Systems/MatrixOS.git>Oct 23, 2026 · @b0b

> **Status (Oct 28, 2026):** this is the original spec, kept for its research.
>
> **Superseded:** the app was built with the owner's own layout instead of section 3. The current controls are in `Applications/SC1000/README.md`.
>
> **Still accurate and useful:**
> - Section 1 (SC1000 MIDI)
> - Section 2 (hookup)
> - Section 5 (extended map)
> - Phase 2
> - The sample prep script
>
> **Dropped from section 3:** cut pads, volume faders, start/stop, random, record and the sample-root setting.
>
> **Milestones:**
> - Done: 1–6 (the app is tested and working on a real SC1000).
> - Still open: Phase 2 and the sample prep script.

A plan for Claude Code to build a Matrix OS app ("SC1000") that turns the Mystrix Pro into a one-way MIDI controller for the Portablism SC1000/SC500. No code here — just the spec, the verified MIDI implementation, and milestones.

## 1. What the SC1000 actually does with MIDI

Verified by reading the SC1000 firmware source (rasteri/SC1000, v1.7: `software/scsettings.txt`, `sc_midimap.c`, `xwax.c`, `sc_input.c`).

**Connection**

- MIDI input is USB host only. The SC1000 runs Linux and opens any class-compliant USB MIDI device as an ALSA rawmidi port. No DIN MIDI.
- It scans for MIDI devices once, `mididelay` seconds after boot (default 5). Plug the Mystrix in before powering the SC1000, or raise `mididelay`.
- It never sends MIDI back. There is no LED feedback, no state readout — the app has to keep its own mirror of state.
- The map lives in `scsettings.txt` on the USB stick as `midii=status,channel,data,edge,action` lines. The channel field is 0-based (0 = MIDI channel 1). Data `255` means all notes on that channel.

**Default map (the shipped "Lemur preset")** — channels below are human 1–16:

| MIDI message | Action |
| --- | --- |
| Ch 1, CC 0 | Beats deck volume (value / 128) |
| Ch 2, CC 0 | Samples deck volume (value / 128) |
| Ch 1, pitch bend | Beats deck pitch |
| Ch 2, pitch bend | Samples deck pitch |
| Ch 1, any note on | Beats deck cue point (note number = cue ID) |
| Ch 2, any note on | Samples deck cue point (note number = cue ID) |
| Ch 1/2, any note on while shifted | Delete that cue |
| Ch 4, any note on | Samples deck musical pitch (note 60 = original pitch) |
| Ch 5, note 0 / 1 | Start/stop beats / samples (toggle) |
| Ch 5, note 2 / 3 | Next file beats / samples |
| Ch 5, note 4 / 5 | Next folder beats / samples |
| Ch 5, note 6 / 7 | Previous file beats / samples |
| Ch 5, note 8 / 9 | Previous folder beats / samples |
| Ch 5, note 10 / 11 | Random file beats / samples |
| Ch 5, note 126 | Record |
| Ch 5, note 127 on / off | Shift on / shift off |

**Behaviour details that matter for the app**

- Cues: first press of a note sets that cue at the current position; later presses jump to it. Cues are saved to a `.cue` file next to each track and reload when the file changes, so the app can't know which cues exist after a file change.
- Pitch bend uses full 14-bit resolution, centre 8192, range ± `pitchrange`% (default 50%). CC-based pitch would only be 7-bit — use pitch bend.
- Volume CC: the value is the level (0–127 → 0–\~1.0). Matching ignores the value, so every CC 0 message on that channel sets volume.
- Channel-4 notes set a persistent semitone pitch (equal temperament, 60 = 0). Note-off does nothing, so "return to normal" means sending note 60. This multiplies with the pitch-bend pitch.
- Start/stop is a toggle, not separate start and stop. The app can't know the true play state.
- Note-offs are ignored except where explicitly mapped (only shift-off by default). Zero-velocity note-ons count as note-offs.
- Shift is one global flag shared with the SC1000's own shift button. While shifted, every incoming message is looked up in the shifted mappings, so unmapped shifted messages do nothing.
- Record always records into the beats deck, and only if the samples deck has files.
- Not reachable over MIDI with the default map: jog-wheel pitch mode, jog reverse, the crossfader, the platter. The first two can be added with custom map lines (section 5). A `BEND` action exists in the code but has no config keyword, so it can't be mapped.
- Firmware caveat: there is also a large community rewrite (lodsb/SC1000, 2026) that may change MIDI. This plan targets rasteri v1.7 — check which firmware is on the unit.

**More firmware details**

- Changing files (prev/next/random/folder) resets that deck's pitch-bend pitch and note pitch back to normal.
- The SC1000 doesn't clear cues before loading a new file. If the new sample has no `.cue` file, the previous sample's cues carry over (the sample prep script section has the fix).
- Incoming MIDI is handled outside the audio thread with a single pending-command slot. Messages that arrive faster than it processes them overwrite each other, so senders should avoid floods.

## 2. Hardware hookup

- Decided: the Mystrix plugs straight into the SC1000's USB host port (USB-C → USB-A). The Mystrix is a class-compliant USB MIDI device, so no driver work is needed.
- The RK-006 can't sit in the middle of this setup: the SC1000 has no DIN input, and the Mystrix has to be the USB device the SC1000 hosts.
- If the SC1000's USB port is also used by the sample stick, or the Mystrix's LEDs draw more than the port supplies, use a small powered USB hub. Turn the Mystrix's LED brightness down when running on SC1000 power.
- In the app, send on `MIDI_PORT_USB` only (Matrix OS `MatrixOS::MIDI::Send(packet, MIDI_PORT_USB)`), so nothing leaks to Bluetooth or other ports.

## 3. App design

One Matrix OS application, `SC1000`, with two pages and a settings menu on the function key. All outgoing MIDI is built with `MidiPacket::NoteOn / NoteOff / ControlChange / PitchBend`.

**Page 1 — Perform (8×8)**

| Rows | Function |
| --- | --- |
| 1–2 | 16 sample-deck cue pads (ch 2, notes 0–15) |
| 3 | 8 beat-deck cue pads (ch 1, notes 0–7) |
| 4 | Sample cut pads: gate and kill modes (ch 2, CC 0) |
| 5 | Transport: start/stop ×2, prev/next file ×2, random sample, shift |
| 6 | Pitch: nudge −/+, fine/coarse, reset to 0 for each deck |
| 7–8 | Pressure volume faders for each deck (8 pads each, ch 1/2 CC 0) |

- Cue pads: dim = unknown/unset, bright = set this session. Holding the app's shift pad turns cue presses into deletes (sends shift on, the cue note, shift off) and clears the pad's LED.
- Cut pads use the pad pressure. Gate mode: silent until held, pressure sets volume. Kill mode: mutes while held, restores the stored level on release. These act like a transformer button and work on top of the hardware fader.
- Volume faders: tap a pad to jump the level; pressure while held gives smooth 0–127. Send only on change, rate-limited to around 100–200 messages per second.
- Pitch: the app keeps a local pitch value per deck and sends 14-bit pitch bend. Nudge pads step by a setting (e.g. 0.1% fine, 1% coarse); reset sends 8192.
- Record sits behind shift (shift + random) so it can't be hit by accident.
- After any prev/next/random/folder press on a deck, that deck's cue LEDs go back to "unknown".

**Cut pad modes explained**

- **Gate:** the deck is silent until a cut pad is held; pressure sets how loud. Release = silent again. Tapping while moving the platter gives transformer-style cuts without touching the fader.
- **Kill:** the deck plays normally; holding a pad mutes it, release restores the stored level.
- Both sit on top of the hardware crossfader, so in gate mode the fader must be open to hear anything.
- Gate leaves the deck at volume 0 when nothing is held, so the app must restore the stored volume when switching modes, changing pages or exiting.

**State mirror after file changes**

- When the app sends prev/next/random/folder for a deck, it also resets its own pitch value to 0% (8192) and note to 60 for that deck, matching what the SC1000 does, and sets that deck's cue LEDs back to "unknown".

**Page 2 — Keys (8×8)**

- Chromatic, isomorphic layout sending ch 4 notes for musical scratching/pitch play on the samples deck. Note 60 is highlighted as the original pitch.
- Settings: row offset (e.g. fourths or semitones) and octave shift.
- One pad is a dedicated "original pitch" key that sends note 60.

**Scale mode (Keys page)**

- Settings: *sample root* (the note the sample is tuned to, default C), *key* and *scale* (major, natural minor, pentatonics, blues, dorian, chromatic, etc.).
- Only in-scale notes go on the grid, laid out in rows like the Note app (e.g. each row starts a 4th or an octave up). Root notes lit in one colour, other scale notes in another.
- Each pad's MIDI note = `60 + (target pitch − sample root)` in semitones. So with root C, the pad for D sends 62, E sends 64. With root A, the pad for C sends 63.
- Playable range is about ±2 octaves around the sample's original pitch; outside that the sample gets very fast or very slow and grainy. Grey out pads beyond a set range (default ±24 semitones).
- This is repitching, not time-stretching: higher notes also play faster.
- Tuning only holds if the pitch-bend pitch is at 0%. Entering the Keys page (or pressing a pad on it) resets that deck's pitch bend to 8192 first, or the Perform page shows a "detuned" warning colour.
- Note-off does nothing on the SC1000, so the last note stays until another is pressed. The "original pitch" pad sends note 60.

**Touch strips**

- Left strip = beats volume, right strip = samples volume (8 coarse steps are fine for volume). Optional — off by default.

**Settings menu (function key)**

Saved with `CreateSavedVar` so they survive restarts.

- Channel offsets (in case the scsettings map is customised)
- Pitch range mirror (for display only)
- Cut mode: gate or kill, and the level used. Saved, so it stays on whichever you last picked (gate on first run)
- Pressure on/off for faders and cut pads
- Keys layout: row offset, octave
- Touch-strip volume on/off
- "Clear state mirror" (resets all LEDs to unknown)
- Page switching and exit follow the Example app's conventions (tap function key = menu, hold = exit).

## 4. Matrix OS notes for Claude Code

From the current MatrixOS repo (203-Systems/MatrixOS):

- Read `AGENT.md` first, then `OS/MatrixOS.h` (the app-facing API), `Applications/Application.h` and `Applications/Example/`.
- Apps derive from `Application` with `Setup / Loop / End`, an `Application_Info` struct, and register through CMake (`RegisterApplicationClass`) plus the device family's `ApplicationList.txt` — don't hand-edit generated files.
- Key events carry `pressure` and `velocity` (`Fract16`) in `KeypadInfo`.
- Check whether `MidiPacket` channel arguments are 0-based before writing the channel constants.
- The Mystrix device family is `Devices/Mystrix1`. `MystrixSim` exists and may be useful for testing the layout before flashing.

## 5. Optional: extended scsettings.txt map

Adds jog-wheel features the default map can't reach. Append to the MIDI section of `scsettings.txt` on the SC1000's stick. Channel field is 0-based, so `4` = MIDI channel 5.

```
# Hold note 12 on ch5 = platter controls beats pitch, release = normal
midii=9,4,12,1,CH0_JOGPIT
midii=8,4,12,1,CH0_JOGPSTOP
# Hold note 13 on ch5 = platter controls samples pitch
midii=9,4,13,1,CH1_JOGPIT
midii=8,4,13,1,CH1_JOGPSTOP
# Note 14 on ch5 = toggle jog reverse
midii=9,4,14,1,CH1_JOGREVERSE
```

If this map is used, the app gets two extra hold pads on the Perform page ("jog pitch" per deck) and a jog-reverse toggle in settings. Build the app so these pads only appear when an "extended map" setting is on.

## 6. Milestones

1. **Set up.** Clone MatrixOS, build and flash the Example app for the Mystrix Pro to confirm the toolchain works.
2. **Skeleton.** Register an empty `SC1000` app that sends one test note on `MIDI_PORT_USB`. Verify with a MIDI monitor on a computer (channel and note numbers exactly as in section 1).
3. **Perform page.** Cue pads with LED mirror, transport, shift, pitch nudge, volume faders, cut pads.
4. **Keys page.** Chromatic layout on ch 4, original-pitch key.
5. **Settings and persistence.** Menu, saved vars, clear-mirror.
6. **Hardware test on the SC1000.** Check every row of the section 1 table, cue set/jump/delete, pitch bend range at both ends, cut-pad latency, and that nothing floods the SC1000 when pressure is held.
7. **Optional.** Extended map pads (section 5), touch-strip volume.

**Done when:** every default-map action is reachable from the grid, cut pads feel instant, pitch returns exactly to 0% on reset, and the app never sends anything the SC1000 doesn't map.

## Phase 2 — Cut sequencer

Start only after Phase 1 (milestones 1–6) works on the real SC1000. Adds a third page that sequences the deck volume, so the grid plays fader cuts in time.

**How it works**

- Each step sends deck volume (CC 0) on that deck's channel: step on = open, step off = closed. The SC1000 treats this like a fader moving.
- The SC1000 has no MIDI clock in or out, so tempo comes from the app: set the BPM, tap tempo, and nudge ± to drift into line with the beat deck by ear.
- A **restart** pad jumps back to step 1 the moment it's pressed, for lining up with the downbeat live.

**Page 3 — Cuts (8×8)**

| Rows | Function |
| --- | --- |
| 1–2 | 16 steps for the samples deck (tap to toggle; press harder for a louder step) |
| 3–4 | 16 steps for the beats deck (optional lane, off by default) |
| 5 | 8 pattern slots (tap to switch, switches on the next bar) |
| 6 | Step rate (1/8, 1/16, 1/32, triplets), pattern length, swing |
| 7 | Play/stop, tap tempo, nudge −/+, restart |
| 8 | Playhead and BPM readout |

**Rules**

- Send a CC only when the level changes. Runs of open or closed steps send nothing, which keeps traffic low for the SC1000's one-message queue.
- Enforce a minimum gap between messages (start at \~5 ms) and test the fastest rate at the highest BPM to find where steps start getting dropped.
- Optional gate length per step (e.g. 25 / 50 / 100% of the step) for choppier cuts.
- Holding a manual cut pad on the Perform page overrides the sequencer for that deck while held.
- On stop, on leaving the page and on exiting the app, restore each deck's volume to its stored level.
- Use a proper timer for step timing rather than the app loop, and check its resolution in Matrix OS.
- Save patterns to the filesystem so they survive restarts.

**Possible later addition:** a pitch lane that sequences scale notes (ch 4) from the Keys page settings, for melodic sample patterns.

**Phase 2 milestones**

1. Timer and transport: play/stop, BPM, tap tempo, nudge, restart, playhead on the grid.
2. Samples lane sending volume CCs, tested on the SC1000 at slow and fast rates.
3. Step levels from pressure, gate length, swing, step rate.
4. Pattern slots and saving.
5. Beats lane and manual-pad override.

**Done when:** a 16-step pattern stays in time against the beat deck at your usual tempos with no dropped cuts, and stopping always leaves the decks audible.

## Sequencing cuts from external gear

Not part of the app, but the same mechanism works from other gear: anything that sends CC 0 on channel 2 (samples) or channel 1 (beats) to the SC1000 moves that deck's volume like a fader.

- The SC1000 only accepts USB MIDI, so DIN-only gear needs a class-compliant USB MIDI interface plugged into the SC1000. The RK-006 might work for this but is untested with the SC1000.
- Several USB MIDI devices can share the SC1000 through a hub (it picks up all of them at boot), so the Mystrix and a sequencer can both be connected.
- Keep patterns sparse (one message per change) because of the single-slot MIDI queue. Smooth CC streams or very fast patterns can drop steps.
- No MIDI clock in or out, so tempo is matched by ear.

## Sample prep script (separate tool for Claude Code)

Goal: use the SC1000 as a strange sampler rather than only a scratch tool. A reusable command-line script, run on a computer before copying samples to the SC1000's USB stick. Works on one file or a whole folder.

**Tuning to C**

- Detect pitch across the recording with librosa's pYIN and take the median, so small wobbles don't throw it off.
- Work out the offset in cents to the nearest C (any octave is fine — scale mode only cares that it's a C). Allow a different target note as an option.
- Shift by that amount. Default: resample (changes length slightly, sounds cleanest, matches how the SC1000 repitches). Option: time-preserving shift with Rubber Band.
- Optional: flatten pitch drift onto C over time (gentle auto-tune) for wobbly whistles.
- Print a report: detected pitch, cents off, shift applied.
- Save as 16-bit WAV at the SC1000's sample rate (44.1 kHz reported for the SC1000 mk2; confirm on the unit).

**Writing cue points (`.cue` files)**

- File: same folder and base name as the sample, `.cue` extension (`whistle.wav` → `whistle.cue`). Avoid two samples with the same base name in one folder.
- Format: plain text, one number per line, position in seconds from the start. Line 1 = cue 0 = MIDI note 0 = first cue pad; line 2 = cue 1, etc.
- Always write all 512 lines, with `inf` for unused cues, for every sample. This stops the previous sample's cues carrying over.
- Never put cue 0 at exactly 0.0 (the firmware then skips saving live cue changes); use 0.001.
- Cue modes: transient detection (librosa onset detection), N equal slices, or times given by hand.
- Live cue presses on the SC1000 still work and will rewrite the `.cue` file.

**Done when:** a folder of raw recordings comes out tuned to C with sliced cue files, plays in tune on the Keys page, and every slice is on its own cue pad.

## 7. Things to check before starting

- Which firmware is on the SC1000 (rasteri 1.7 vs the lodsb rewrite) and whether its `scsettings.txt` contains the `midii=` lines above.
- How many USB ports the SC1000 has and whether the sample stick shares the one the Mystrix needs.
- Whether the Mystrix runs reliably on SC1000 USB power at the brightness you like.

## 8. Prompt to give Claude Code

> Build a new Matrix OS application called SC1000 for my Mystrix Pro, following the plan in this doc. Start by reading AGENT.md, OS/MatrixOS.h and Applications/Example in the MatrixOS repo. Work through the milestones in order and stop after milestone 2 so I can verify the MIDI output with a monitor before you build the pages. Don't start the Phase 2 cut sequencer until I confirm Phase 1 works on the SC1000.

For the sample prep script, a separate prompt:

> Write a reusable Python command-line script that prepares samples for my Portablism SC1000, following the "Sample prep script" section of this doc. It should tune a file or folder of recordings to C (or another note), save 16-bit WAVs, and write a full 512-line .cue file for each sample using transient detection, equal slices or times I give it.

## Sources

- [rasteri/SC1000 on GitHub](https://github.com/rasteri/SC1000) — firmware source, `software/scsettings.txt` and `software/sc_midimap.c`
- [SC1000 releases](https://github.com/rasteri/SC1000/releases)
- [203-Systems/MatrixOS on GitHub](https://github.com/203-Systems/MatrixOS)
