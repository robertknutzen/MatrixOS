// Chord Machine: plays the Elektron Syntakt's SY CHORD machine.
// Each chord sends CC 21 (chord TYPE) and CC 22 (BAL = inversion), then the root note.
//
// Built like the Note app: each screen is a UI made of components, and MatrixOS handles drawing and input.
// The app itself only keeps the settings and does the music and MIDI work, so any screen can play chords.
#pragma once

#include "MatrixOS.h"
#include "Application.h"
#include "Scales.h" // From Applications/Note

class ChordMachine : public Application {
public:
  inline static Application_Info info = {
      .name = "Chord Machine",
      .author = "b0b",
      .color = Color(0xFF6000),
      .version = 3,
      .visibility = true,
  };

  static constexpr uint8_t CC_CHORD_TYPE = 21; // SYN page, data entry knob E (TYPE)
  static constexpr uint8_t CC_BALANCE = 22;    // SYN page, data entry knob F (BAL)
  // Same knobs as NRPN (manual Appendix B.3): MSB 1, LSB 5 / 6
  static constexpr uint8_t NRPN_MSB = 1;
  static constexpr uint8_t NRPN_CHORD_TYPE = 5;
  static constexpr uint8_t NRPN_BALANCE = 6;
  static constexpr int8_t OCTAVE_MIN = -3;
  static constexpr int8_t OCTAVE_MAX = 2;
  static constexpr uint8_t BASE_NOTE = 48; // Octave 0 puts the key root at C3

  // A triad on one scale degree, ready to play
  struct Chord {
    bool valid = false; // False when no triad fits the scale there, or the note is out of range
    uint8_t type = 0;   // Syntakt TYPE index
    uint8_t balance = 0; // BAL value for the inversion
    uint8_t note = 0;   // Root note sent
    uint8_t degree = 0; // Scale degree (0 = key root)
    Color color;        // By quality: major, minor, diminished, augmented
  };

  void Setup(const vector<string>& args) override;
  void End() override;

  // Music and MIDI, shared by every screen
  // step counts up the scale from the key root (past the last degree it wraps an octave up)
  Chord GetChord(uint8_t step, uint8_t inversion, int8_t octaveShift = 0);
  void ChordOn(const Chord& chord, uint8_t velocity);
  void ChordOff(const Chord& chord);
  void AllNotesOff();
  int8_t Octave() { return octave.Get(); }
  void ChangeOctave(int8_t direction);

private:
  CreateSavedVar("ChordMachine", keyRoot, uint8_t, 0);
  CreateSavedVar("ChordMachine", scale, uint16_t, MAJOR);
  CreateSavedVar("ChordMachine", channel, uint8_t, 0); // 0-based: 0 = MIDI channel 1
  CreateSavedVar("ChordMachine", octave, int8_t, 0);
  CreateSavedVar("ChordMachine", useNrpn, bool, false); // Send TYPE and BAL as NRPN instead of CC

  void SendParameter(uint8_t cc, uint8_t nrpnLsb, uint8_t value);

  // Knob twist test: sends TYPE Fifth, minor, Fifth with a gap, as if the knob were turned by hand
  static constexpr uint8_t TYPE_FIFTH = 37;
  static constexpr uint8_t TYPE_MINOR = 3;
  static constexpr uint32_t TWIST_GAP_MS = 100;
  static constexpr uint8_t TWIST_VALUES[3] = {TYPE_FIFTH, TYPE_MINOR, TYPE_FIFTH};
  uint8_t twistStep = 3; // 3 = idle
  uint32_t twistNextMs = 0;
  void StartTwist();
  void TwistTick();

  // The scale as semitones above the key root; rebuilt when the key or scale changes
  uint8_t steps[12] = {};
  uint8_t stepCount = 0;
  uint16_t scaleMask = 0; // Absolute pitch classes in the key
  void UpdateScale();

  uint8_t noteCount[128] = {}; // Pads holding each note, so a note only stops when its last pad lifts

  // Screens
  void PlayView();
  void SettingsMenu();
  void ScalePage();
};
