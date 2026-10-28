# SC1000 app: scale keys on the Perform page (built)

How the scale keys ended up, and what was reused from the Note app. The current controls are in `Applications/SC1000/README.md`.

## What was built
Perform rows 6–8 are scale-locked keys that repitch the samples deck with note-ons on ch 4 (60 = original pitch).

- **Octave rows:** one octave per row, like the Note app's octave layout. The middle row starts on the key root nearest note 60 (`60 + key` for keys C–F#, `48 + key` for G–B). The top row is an octave up, the bottom row an octave down. Each row walks up the scale for 8 pads.
- **Range:** notes more than ±24 semitones from 60 are dark and do nothing.
- **Tuning assumption:** the sample is tuned to C, so the pad's MIDI note is sent as-is. The spec's separate "sample root" setting and its `60 + (pitch − sampleRoot)` mapping were dropped. The sample prep script (still to do) is meant to tune samples to C.
- **Pressing a key:**
  - It resets the samples pitch bend to 0% first, because the tuning only holds at 0%.
  - It sends one note-on through the app's 5 ms spaced queue. No note-off is sent, so the note stays.
- **Original Pitch** (red, row 5 col 1) sends note 60. Samples RESET on Main also sends note 60, along with the pitch reset.
- **Lights:** root notes brighter than other scale notes, the current note full white.
- **Cue pads ignore the scale.** Cue n is always note n.
- **Settings:** key and scale are picked on Settings → Scale and saved. The key map is rebuilt when that page closes.

## Reused from the Note app
Through `${CMAKE_CURRENT_SOURCE_DIR}/../Note` in SC1000's include dirs (all header-only):
- **`Scales.h`:** the `EScale` 12-bit masks.
- **`ScaleVisualizer.h` (`NoteScaleVisualizer`):** the piano-shaped key picker. A dummy `rootOffset` stays 0. The header has no `#pragma once`, so include it once.

From the OS:
- **`UIItemSelector<uint16_t>`:** the 16-scale picker.

Copied rather than included:
- **The 16-scale list and names**, from `Note.h`. Including `Note.h` would pull in the whole app.
- **The key rotation:** `aligned = ((scale << key) + ((scale & 0xFFF) >> (12 − key))) & 0xFFF`.

## Deliberately not reused
**The `NotePad` component.** It sends on every port class, sends note-offs and aftertouch, and bypasses the spaced queue, so a chord would overwrite itself in the SC1000's one-message buffer.
