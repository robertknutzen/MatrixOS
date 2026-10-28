# SC1000 app: reusing the Note app's scale code for the Keys page (notes, not built yet)

## Context
The spec's Keys page (`SC1000 Controller app for Mystrix Pro — build plan.md`, "Page 2 — Keys" and "Scale mode") needs:
- sample root, key and scale settings
- in-scale notes laid out in rows like the Note app, with root notes in one colour and other scale notes in another
- each pad sends `60 + (target pitch − sample root)` on ch 4 (0-based channel 3)
- pads more than ±24 semitones from the sample's pitch greyed out

The Note app (`Applications/Note/`) already has most of the scale pieces. The user said to wait for their screen designs before building. These notes record what can be reused.

## Reuse directly (include from `../Note`, like `BPMTapper.h` in the tempo notes)
Add `${CMAKE_CURRENT_SOURCE_DIR}/../Note` to SC1000's `target_include_directories`. These are all header-only and don't need the Note library.

- **`Applications/Note/Scales.h`**: the `EScale` enum of 32 scales as 12-bit masks (bit 0 = C … bit 11 = B, written for root C). Includes MAJOR, MINOR, DORIAN, BLUES, MINOR_PENTATONIC, MAJOR_PENTATONIC, CHROMATIC and others. It covers every scale the spec lists.
- **`Applications/Note/ScaleVisualizer.h`** (`NoteScaleVisualizer`): a 7×2 piano-shaped picker for the root/key. It shows the selected scale on a keyboard layout; tapping a key sets `*rootKey` (0–11). It takes pointers `(uint8_t* rootKey, uint8_t* rootOffset, uint16_t* scale)` plus colours, and has an `OnChange` callback.
  - Use one for **Key**, and a second for **Sample root** (pass a CHROMATIC scale pointer so every key looks selectable).
  - `rootOffset` (Note's "diatonic mode") isn't needed. Pass a dummy `uint8_t` that stays 0 and leave `offsetMode` false.
  - Caveat: the file has no `#pragma once`, so include it only once.
- **`UIItemSelector<uint16_t>`** (`OS/UI/Component/UIItemSelector.h`, an OS component): the 8×4 scale-list picker used in `Note::ScaleSelector()` (`Note.cpp` ~407). It takes `SetItems(array)`, `SetCount`, `SetItemPointer(&scale)` and `SetIndividualNameFunc` for scrolling names.

## Copy (small, tied to Note's classes)
- **The scale list and names**: `Note::scales[16]` and `Note::scale_names[16]` (`Note.h` ~69) are static members of the Note app class, and including `Note.h` would pull in the whole app. Copy them into SC1000 as its own 16-entry array. That's also a chance to swap in scales the Note list lacks but the spec names, e.g. `MAJOR_PENTATONIC` is in `Scales.h` but not in Note's 16.
- **Shifting the scale to the key** (`NotePad.cpp` `GenerateKeymap()`, line ~370):
  `aligned = ((scale << rootKey) + ((scale & 0xFFF) >> (12 - rootKey % 12))) & 0xFFF;` rotates the C-based mask to the chosen key. `bitRead(aligned, note % 12)` then tells whether a note is in the scale.
- **In-scale tests** (`NotePad.cpp` `InScale()` and `GetNextInScaleNote()`, lines ~40–65): about 20 lines. Copy them as free functions taking `(aligned, rootKey)` instead of `NotePad` members.
- **Row layout** (`NotePad.cpp` `GenerateOffsetKeymap()`, the `enforceScale` branch, ~line 245): this is the "only in-scale notes, each row starts N scale steps up" layout the spec wants. With `xOffset = 1` and `y_offset = 3`, each row starts a 4th up in a 7-note scale. Like Note, it builds rows from the bottom up (`uiY = dimension.y - y - 1`). Copy the loop and change it to:
  - store the musical pitch per pad, then convert it with `scNote = 60 + (pitch − sampleRootPitch)`
  - mark pads with `|pitch − sampleRootPitch| > 24` as greyed-out (`255`)
  - set the start pitch from key, octave and sample root, so the original-pitch note (SC note 60) lands somewhere obvious

## Don't reuse
- **The `NotePad` UI component itself.** It would be tempting to drop the Note app's pad grid in whole, but:
  - it sends through its own pipeline straight to `MIDI_PORT_EACH_CLASS` (USB + DIN + Bluetooth), not USB only;
  - it sends note-offs and poly aftertouch, which the SC1000 doesn't map;
  - it doesn't use SC1000's 5 ms spaced queue, so a chord would overwrite itself in the SC1000's one-message buffer.
  The Keys page should send only one note-on per press through SC1000's existing `Queue()`.
- **`NoteScaleModifier`** (custom scale editor): skip unless the user wants custom scales.

## SC1000-specific logic to write
- Saved vars: `sampleRoot` (0–11, default 0 = C), `keyRoot` (0–11), `scaleIndex`, `rowOffset`, `octave`.
- Mapping: `scNote = 60 + (padPitch − sampleRootPitch)`, sent as `NoteOn(CH_KEYS, scNote, 127)` via `Queue()`. No note-off (the SC1000 ignores it).
- The "original pitch" pad always sends 60.
- On entering the Keys page (or on the first Keys press), reset the samples deck pitch bend to 8192 first (`SetPitch(DECK_SAMPLES, 0)`), so the tuning holds as the spec says.
- After a samples file change, the SC1000 resets note pitch to 60. The app's mirror should match (Keys page highlight back to "original").

## Verification (when built)
- With sample root C and key C major: the pad for D sends ch 4 note 62, E sends 64, the original-pitch pad sends 60.
- With sample root A: the pad for C sends 63 (spec's example).
- Pads beyond ±24 semitones are dark and send nothing.
- Check with `aseqdump -p 32:0`: one note-on per press, no note-offs, no aftertouch.
