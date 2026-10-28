# SC1000 app: internal MIDI clock and Tempo page (notes, not built yet)

## Context
The user wants a MIDI clock in the SC1000 app, with the current tempo shown in BPM, reusing code from other apps.
- The user's choices: **the Mystrix is the clock** (internal tempo, nothing external receives it yet; it's for the app itself, e.g. the Phase 2 cut sequencer), and the BPM shows on a **dedicated Tempo page**.
- The clock still goes out on USB. I checked rasteri/SC1000 `dicer.c`: it only parses status bytes 0x80/0x90/0xB0/0xE0 and drops 0xF8/0xFA/0xFC, so clock never reaches its one-command queue. USB-MIDI packets are whole messages, so clock can't land in the middle of another message.
- The user is redesigning the Perform layout (see memory `sc1000-app-progress`). This change only adds the Tempo page and the clock. Opening the page with a tap of the function key is temporary until the user's layout says otherwise.

## Reuse
- `UI4pxNumber` (`OS/UI/Component/UI4pxNumber.h`): 3-digit number display. Same setup as the Note app's BPM menu (`Applications/Note/Note.cpp` ~1170): `SetDigits(3)`, `SetAlternativeColor(White)`, placed at `Point(-1, 0)` so 3 digits fit in 8 columns.
- `NoteBPMTapper` (`Applications/Note/BPMTapper.h`): a header-only tap-tempo component using only MatrixOS and UI. Reuse it by adding `${CMAKE_CURRENT_SOURCE_DIR}/../Note` to SC1000's `target_include_directories`. Don't copy it or link the Note library.
- `UINumberModifier` (`OS/UI/Component/UINumModifier.h`): an 8-pad −/+ row. Use the Note app's `coarseModifier {-25,-10,-5,-1,1,5,10,25}` and `modifierGradient`, limited to 20–299 BPM.
- `UI` page with `SetLoopFunc`, `SetInputEventHandler` and `Start()`, the pattern from the Dice/Note menus.
- **Don't reuse** `Applications/Note/MidiClock`. Its `Tick()` sets `last_tick = micros` on every tick, so lateness adds up and the tempo runs slow. Write a small drift-free clock in SC1000 instead.

## Changes (Applications/SC1000 only)
1. **`CMakeLists.txt`**: add the `../Note` include directory.
2. **Clock (in `SC1000.h/.cpp`)**: 24 PPQN.
   - Saved var `CreateSavedVar("SC1000", bpm, uint16_t, 120)`.
   - State: `clockRunning`, `nextTickUs`, `tickCount`.
   - `ClockTick()` runs every loop. While `now - nextTickUs` ≥ 0: send `MidiPacket::Clock()` on `MIDI_PORT_USB` directly, not through the 5 ms command queue, because clock must be on time and the SC1000 ignores it. Then `nextTickUs += 60000000 / (bpm * 24)` and `tickCount++`. If more than 2 pulses behind, resync to `now`.
   - `ClockStart()` sends `MidiPacket::Start()`, zeroes `tickCount` and sets `nextTickUs = now`. `ClockStop()` sends `Stop()`.
   - Changing BPM recomputes the pulse length without resetting the phase.
   - `End()` stops the clock if it's running.
3. **Main loop**: `Loop()` calls `ClockTick()` before `FlushOutput()`.
4. **Tempo page (`TempoPage()`)**: a `UI` named "Tempo" in the app colour.
   - Rows 0–3: `UI4pxNumber` showing `bpm`, plus `NoteBPMTapper` (8×4) on the same area, so tapping the number taps tempo. Its `OnChange` sets `bpm`.
   - Row 4: `UINumberModifier` for −/+ BPM.
   - Row 6: a beat light, lit on beat 1 and dim on other beats while running (`tickCount / 24 % 4`).
   - Row 7: pad 0 starts/stops the clock (green when running, dim when stopped).
   - `SetLoopFunc` calls `ClockTick()` and `FlushOutput()`, so the clock and queued commands keep running while the page is open.
   - Input handler: a function key tap or release closes the page, and holding it exits the app (Dice pattern).
   - The value pointer is an `int32_t bpmValue` copy, as in the Note app, written back to `bpm` on change.
5. **Opening the page**: on the Perform page, tapping the function key (released without a hold) opens `TempoPage()`. Holding it still exits.

## Verification
1. `make DEVICE=Mystrix MODE=DEVELOPMENT build`, then regenerate the uf2 (see memory `mystrix-build-flash`). It must build with no SC1000 warnings.
2. The user enters bootloader mode and I copy the uf2 over.
3. Run `aseqdump -p 32:0` on this computer while the user opens the Tempo page, starts the clock at 120 and then changes the tempo:
   - Expect `Start`, then a steady stream of `Clock` messages, then `Stop`.
   - Count the clock timestamps with `aseqdump`'s output piped through `ts` (or log for 10 s and count). At 120 BPM, 10 s should give 480 ± 1 ticks. At 90 BPM, 360.
4. Check that the BPM number updates from tap tempo and −/+, and stays after restarting the app (saved var).
5. Check that the Perform page still works with the clock running: cue presses still arrive between clock messages.

## Beat repeat pad (decided with the user, not built yet)
Hold a pad: capture the current position into a scratch cue, then jump back to it every beat until the pad is released. It depends on the internal clock above.

**Decisions**
- **Immediate, not quantized.** Set the cue the moment the pad is pressed. Repeat every beat length measured **from the press**, not on the clock's beat boundaries. The user's reasoning: the SC1000 setup is played by ear and drift against the sample is unavoidable, so an accurate beat length matters more than grid alignment.
- **No delete before the set.** It adds about 15 ms of latency. Accept the risk that the cue is already set (from a crash mid-hold, or a `.cue` file defining it), in which case the first press jumps to the old point.

**Behaviour**
1. Press: `Queue(NoteOn(deck, 65, 127))` sets cue 65 (empty, so it captures the current position). Record `repeatStartUs = now`.
2. While held: retrigger cue 65 every `60,000,000 / bpm` µs after the press. Track it as `nextRepeatUs += beatUs` (drift-free, same approach as the clock tick) rather than counting from `tickCount`, so the first repeat is exactly one beat after the press. BPM changes while held take effect from the next repeat.
3. Release: `Queue` shift on (ch 5 note 127), cue 65, shift off. That deletes the cue so it's empty next time.
- Scratch cue: 65 (outside any cue-pad bank such as 0–15). Make it a constant `CUE_BEAT_REPEAT = 65`.
- Deck: probably the beats deck (ch 1). Confirm from the user's layout. Repeat-rate pads (1, ½, ¼ beat) are just `beatUs / n`; add them only if the layout asks.
- Known clash: the delete uses the SC1000's global shift, which is shared with its hardware shift button.
- Each set and delete rewrites the track's `.cue` file on the SC1000's USB stick. Listen for glitches when testing.

**Verify:** at 120 BPM, holding the pad for 2 s in `aseqdump` shows note 65 at t = 0, then about every 500 ms (0.5, 1.0, 1.5 s), then shift on / 65 / shift off on release.
