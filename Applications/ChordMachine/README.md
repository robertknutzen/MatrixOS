# Chord Machine app for the Mystrix Pro

> **Disabled (2026-10-29).** It's commented out in `Devices/Mystrix1/ApplicationList.txt`. The Syntakt records these chords in Live Recording, but not in grid recording. Holding a step and sending CC, NRPN or a simulated knob twist made no lock. It also isn't great for live playing. If the chord CC arrives while the previous note is still sounding, that note changes chord as it plays, which sounds bad. To turn it back on, uncomment the `ChordMachine` line and rebuild.

Plays the Elektron Syntakt's **SY CHORD** machine. Each pad sends the chord TYPE as a CC, then the root note, so one finger plays any chord in the chosen key and scale.

## Syntakt setup

- Put the SY CHORD machine on a track and set that track's MIDI channel (default: channel 1).
- In MIDI CONFIG, turn on receiving notes and CC/NRPN.
- Connect through a USB host (a computer, or a USB MIDI host box), or through the Mystrix's TRS MIDI out. The app sends to every MIDI output. Both the Syntakt and the Mystrix are USB devices, so they can't be plugged straight into each other.

## Main grid

Columns 1–8 walk up the scale from the key root. Each column plays that degree's triad. Past the last degree they wrap an octave up: in a 7-note scale, column 8 is the root an octave higher.

| Row | What it plays |
|---|---|
| 1 | Octave − (col 1), knob twist test (col 4, blue), octave + (col 8) |
| 2 | — |
| 3 | Inv2, upper octave |
| 4 | Inv2, lower octave |
| 5 | Inv1, upper octave |
| 6 | Inv1, lower octave |
| 7 | Root position, upper octave |
| 8 | Root position, lower octave |

- The triad is the first of Major, minor, mb5 (diminished) or M#5 (augmented) that fits the scale. A column stays dark when none fits, which happens on some degrees of pentatonic and blues scales.
- **Colours:** major is orange, minor blue, diminished red, augmented purple. The key root column is brighter. A held pad goes to full brightness.
- Velocity follows how hard you press.
- **Octave:** range −3 to +2. At octave 0 the lower rows start at C3 (note 48). The octave pads get brighter the further you shift.
- **Knob twist test (row 1, col 4):** sends TYPE = Fifth, then minor, then Fifth again, 100 ms apart, as if the knob were turned by hand. It uses CC or NRPN, whichever the NRPN switch picks. No note is sent. It lights fully while it runs. The test: hold a step, press it, and see if a lock records.
- **Function key:** tap to open Settings, hold to exit.

## Settings (tap the function key)

- **Rows 1–2:** MIDI channel 1–16.
- **Row 8, col 1 (yellow):** scale page. The piano picks the key and the grid picks one of 16 scales.
- **Row 8, col 8 (light blue):** NRPN switch. When it's lit, TYPE and BAL go out as NRPN (address 1/5 and 1/6) instead of CC 21/22. This is a test: does the Syntakt record NRPN as locks when you hold a step in grid recording?
- Tap the function key to go back. Hold it to exit the app.
- The key, scale, channel, octave and NRPN switch are saved.

## MIDI sent

| Action | Message |
|---|---|
| Press a chord pad | CC 21 = chord TYPE index (knob E), CC 22 = BAL for the inversion (knob F), then note-on for the root |
| Release | Note-off, once no other held pad is on that note |
| Leaving the app or opening Settings | Note-off for every sounding note |

TYPE index follows the manual's Appendix D order, starting with Unison ×2 = 0, minor = 3 and Major = 4. The app assumes the CC value is that index.

BAL values are 32 (root), 62 (Inv1) and 72 (Inv2), in `INVERSION_BAL` in `ChordMachine.cpp`. They come from a forum chart of a 4-note chord and still need checking against the Syntakt's BAL display for triads.

## Code

Built like the Note app: each screen is a MatrixOS `UI` made of components, and the OS draws them and routes presses.

- `ChordMachine.cpp`: settings, the music (which triad sits on each scale step) and the MIDI. Also the play screen, `PlayView()`.
- `ChordGrid.h`: a reusable block of triad pads. You give it a list of rows, each with an inversion and an octave shift, and place it anywhere on a screen.
- `PadButton.h`: a one-pad button that acts on press and lights fully while held.
- `ChordMachineMenus.cpp`: the Settings and Scale screens.

To add a screen, write a function like `PlayView()` that builds a `UI` from these parts, then open it from a button or the Settings menu.
