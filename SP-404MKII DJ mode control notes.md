# SP-404MKII DJ mode control from the Mystrix: notes (not planned)

Research only, from `SP-404mk2_v550_reference_eng04_W.pdf` in this folder (MIDI chart pp. 167–169, effect list p. 157, DJ mode pp. 93–98). The user isn't pursuing this for now.

## What the 404 accepts over MIDI in DJ mode

| Control | MIDI | Accepted? |
|---|---|---|
| CH1 / CH2 volume | CC 7 on ch 1 / ch 2 (ch 3 = pattern sequencer volume) | ✓ |
| Crossfader | CC 8 on ch 1 (127:0 → 0:127 = CH1 → CH2) | ✓ |
| Bus FX effect select | CC 83 (BUS 1 = ch 1, BUS 2 = ch 2, BUS 3/4 = ch 3/4, INPUT = ch 5) | ✓ |
| Bus FX on/off | CC 19 (0–63 off, 64–127 on) | ✓ |
| Bus FX knobs Ctrl 1–6 | CC 16, 17, 18, 80, 81, 82 | ✓ |
| MIDI clock in (tempo follow) | clock / start / stop | ✓ |
| Patterns | Program change 0–15 = pattern 1–16 | ✓ |
| Play/pause, cue-return, sync, cue, bend ±, BPM ± | CC 20–27 on ch 1 / 2 / 3 | ✗ **Sent only, not accepted** |

So the Mystrix **can't** play, cue, nudge or sync the 404's DJ decks. Those stay on the 404's pads. The Mystrix would work as an **FX and mixer surface** beside it.

In DJ mode, each deck's routing to BUS-1 / BUS-2 / DRY is set on the 404 with [REMAIN] + pad 14 (CH1) or pad 16 (CH2). Over MIDI you control the effects on those buses.

### BUS 1/2 effects (CC 83 value)
0 off · 1–5 Direct FX1–5 · 6 Scatter · 7 Downer · 8 Ha-Dou · 9 Ko-Da-Ma · 10 Zan-Zou · 11 To-Gu-Ro · 12 SBF · 13 Stopper · 14 Tape Echo · 15 TimeCtrlDly · 16 Super Filter · 17 WrmSaturator · 18 303 VinylSim · 19 404 VinylSim · 20 Cassette Sim · 21 Lo-fi · 22 Reverb · 23 Chorus · 24 JUNO Chorus · 25 Flanger · 26 Phaser · 27 Wah · 28 Slicer · 29 Tremolo/Pan · 30 Chromatic PS · 31 Hyper-Reso · 32 Ring Mod · 33 Crusher · 34 Overdrive · 35 Distortion · 36 Equalizer · 37 Compressor · 38 SX Reverb · 39 SX Delay · 40 Cloud Delay · 41 Back Spin · 42 DJFX Delay

What Ctrl 1–6 do depends on the effect (see the effect parameter pages in the manual).

## Ideas
1. **FX pads:** a grid of favourite effects for BUS 1 and BUS 2. Holding a pad sends CC 83 (select) and CC 19 on; releasing sends CC 19 off, so effects work as momentary hits. **Pad pressure drives an effect knob** (e.g. Ctrl 1 = filter cutoff or delay feedback).
2. **XY strip:** a block of pads as an X/Y pad. Column sets Ctrl 1 and row sets Ctrl 2, so two effect knobs move with one finger.
3. **Crossfader / volume cuts:** transformer-style crossfader cuts (CC 8) and channel kills (CC 7) from pads, like the SC1000 cut pads.
4. **Tempo-locked cut/FX sequencing:** the 404 can send its MIDI clock (System → MIDI Sync Out = ON). With "Bend SYNC (DJ)" = ON, the clock follows its bends. If the Mystrix follows that clock, sequenced cuts, FX bursts and quantized FX pads stay in time with what's playing. This avoids the SC1000 problem, where the app's beat isn't the track's beat. **Needs new clock-receive code:** no MatrixOS app follows incoming clock today (the Note app's "external clock" option is unimplemented).
5. **Status lights:** the 404 sends CC 20–27 when its DJ buttons are pressed, so the Mystrix could mirror play/cue/sync state per deck (one-way, display only).

## Hookup
- The 404's USB-C port and the Mystrix are both USB **devices**, so they can't connect directly.
- Use the 404's TRS MIDI IN/OUT (BOSS BMIDI TRS-to-DIN cables): Mystrix → USB MIDI host interface → DIN → 404 MIDI IN.
- Clock following (idea 4) also needs 404 MIDI OUT → the interface's DIN in, routed back to the Mystrix over USB.
- Send on USB port 1 only (`MIDI_PORT_USB`), as the SC1000 app does, to avoid duplicates (see commit "Send app MIDI once per port class").

## If picked up later
Start with a layout table like the SC1000 one, probably as a separate "SP404" app. Build idea 4 (clock follow) first, since the tempo-locked features depend on it.
