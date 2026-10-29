#include "ChordMachine.h"
#include "ChordGrid.h"
#include "PadButton.h"

namespace
{
struct Triad {
  uint8_t type;   // Syntakt TYPE index (manual Appendix D order, Unison x2 = 0)
  uint16_t notes; // Pitch classes above the root, bit n = n semitones
  Color color;
};

// Checked in this order; the first one that fits the scale is used
const Triad TRIADS[4] = {
    {4, 0b000010010001, Color(0xFF8000)},  // Major: 0 4 7, orange
    {3, 0b000010001001, Color(0x0060FF)},  // minor: 0 3 7, blue
    {17, 0b000001001001, Color(0xFF0020)}, // mb5 (diminished): 0 3 6, red
    {21, 0b000100010001, Color(0xA000FF)}, // M#5 (augmented): 0 4 8, purple
};

// BAL (CC 22) value for root position, Inv1 and Inv2.
// From a forum chart of a 4-note chord; still to be checked against the Syntakt's screen for triads.
const uint8_t INVERSION_BAL[3] = {32, 62, 72};

const Color COLOR_OCTAVE = Color(0xFFFFFF);
const Color COLOR_TWIST = Color(0x0040FF);

// Rotate a C-based mask to start on another pitch class (same formula as NotePad::GenerateKeymap)
uint16_t Rotate(uint16_t mask, uint8_t root) {
  root %= 12;
  return ((mask << root) + ((mask & 0xFFF) >> (12 - root))) & 0xFFF;
}
} // namespace

void ChordMachine::Setup(const vector<string>& args) {
  if (octave.Get() < OCTAVE_MIN || octave.Get() > OCTAVE_MAX)
    octave = 0;
  UpdateScale();
  PlayView(); // Runs until the app exits
  Exit();
}

void ChordMachine::End() {
  AllNotesOff();
}

// ---------- Screens ----------

// Main screen: octave buttons in the top corners, triads in rows 3-8
void ChordMachine::PlayView() {
  UI playView("Chord Machine", info.color, false);

  // Top to bottom: Inv2, Inv1, root position; the upper row of each pair is an octave higher
  ChordGrid grid(this, {{2, 1}, {2, 0}, {1, 1}, {1, 0}, {0, 1}, {0, 0}});
  playView.AddUIComponent(grid, Point(0, 2));

  // Octave pads light up more the further you've shifted that way
  PadButton octaveDown;
  octaveDown.SetColor(COLOR_OCTAVE);
  octaveDown.SetBrightnessFunc([&]() -> uint8_t { return Octave() < 0 ? 80 + 55 * -Octave() : 40; });
  octaveDown.OnPress([&]() -> void { ChangeOctave(-1); });
  playView.AddUIComponent(octaveDown, Point(0, 0));

  PadButton octaveUp;
  octaveUp.SetColor(COLOR_OCTAVE);
  octaveUp.SetBrightnessFunc([&]() -> uint8_t { return Octave() > 0 ? 80 + 55 * Octave() : 40; });
  octaveUp.OnPress([&]() -> void { ChangeOctave(1); });
  playView.AddUIComponent(octaveUp, Point(7, 0));

  // Knob twist test pad (blue)
  PadButton twistButton;
  twistButton.SetColor(COLOR_TWIST);
  twistButton.SetBrightnessFunc([&]() -> uint8_t { return twistStep < 3 ? 255 : 120; });
  twistButton.OnPress([&]() -> void { StartTwist(); });
  playView.AddUIComponent(twistButton, Point(3, 0));

  playView.SetLoopFunc([&]() -> void { TwistTick(); });

  // Function key: tap opens Settings, hold exits the app
  playView.SetInputEventHandler([&](InputEvent* inputEvent) -> bool {
    if (inputEvent->id != InputId::FunctionKey())
      return false;
    if (inputEvent->keypad.state == KeypadState::Hold)
      Exit();
    else if (inputEvent->keypad.state == KeypadState::Released && !inputEvent->keypad.hold)
    {
      grid.ReleaseAll(); // Releases while Settings is open would never reach the grid
      SettingsMenu();
    }
    return true;
  });

  playView.AllowExit(false);
  playView.Start();
}

// ---------- Music ----------

void ChordMachine::UpdateScale() {
  uint8_t root = keyRoot.Get() % 12;
  scaleMask = Rotate(scale.Get(), root);
  if (scaleMask == 0)
    scaleMask = 0xFFF;

  stepCount = 0;
  for (uint8_t i = 0; i < 12; i++)
    if (bitRead(scaleMask, (root + i) % 12))
      steps[stepCount++] = i;
}

ChordMachine::Chord ChordMachine::GetChord(uint8_t step, uint8_t inversion, int8_t octaveShift) {
  Chord chord;
  if (stepCount == 0 || inversion > 2)
    return chord;

  uint8_t root = keyRoot.Get() % 12;
  uint8_t degree = step % stepCount;
  uint8_t pitchClass = (root + steps[degree]) % 12;
  int16_t note = BASE_NOTE + 12 * (octave.Get() + octaveShift + step / stepCount) + root + steps[degree];
  if (note < 0 || note > 127)
    return chord;

  for (const Triad& triad : TRIADS)
  {
    uint16_t notes = Rotate(triad.notes, pitchClass);
    if ((notes & scaleMask) != notes)
      continue;
    chord.valid = true;
    chord.type = triad.type;
    chord.balance = INVERSION_BAL[inversion];
    chord.note = (uint8_t)note;
    chord.degree = degree;
    chord.color = triad.color;
    break;
  }
  return chord;
}

void ChordMachine::ChangeOctave(int8_t direction) {
  int8_t newOctave = octave.Get() + direction;
  if (newOctave >= OCTAVE_MIN && newOctave <= OCTAVE_MAX)
    octave = newOctave;
}

// ---------- MIDI ----------

void ChordMachine::ChordOn(const Chord& chord, uint8_t velocity) {
  if (!chord.valid)
    return;
  // Chord type and inversion first, so the note plays with them
  SendParameter(CC_CHORD_TYPE, NRPN_CHORD_TYPE, chord.type);
  SendParameter(CC_BALANCE, NRPN_BALANCE, chord.balance);
  MatrixOS::MIDI::Send(MidiPacket::NoteOn(channel, chord.note, velocity));
  noteCount[chord.note]++;
}

void ChordMachine::StartTwist() {
  twistStep = 0;
  twistNextMs = (uint32_t)MatrixOS::SYS::Millis(); // First value goes out right away
}

void ChordMachine::TwistTick() {
  if (twistStep >= 3 || (int32_t)((uint32_t)MatrixOS::SYS::Millis() - twistNextMs) < 0)
    return;
  SendParameter(CC_CHORD_TYPE, NRPN_CHORD_TYPE, TWIST_VALUES[twistStep]);
  twistStep++;
  twistNextMs += TWIST_GAP_MS;
}

void ChordMachine::SendParameter(uint8_t cc, uint8_t nrpnLsb, uint8_t value) {
  if (!useNrpn)
  {
    MatrixOS::MIDI::Send(MidiPacket::ControlChange(channel, cc, value));
    return;
  }
  // NRPN: select the parameter (CC 99 / 98), then the value (CC 6 = MSB, CC 38 = LSB)
  MatrixOS::MIDI::Send(MidiPacket::ControlChange(channel, 99, NRPN_MSB));
  MatrixOS::MIDI::Send(MidiPacket::ControlChange(channel, 98, nrpnLsb));
  MatrixOS::MIDI::Send(MidiPacket::ControlChange(channel, 6, value));
  MatrixOS::MIDI::Send(MidiPacket::ControlChange(channel, 38, 0));
}

void ChordMachine::ChordOff(const Chord& chord) {
  if (!chord.valid || noteCount[chord.note] == 0)
    return;
  if (--noteCount[chord.note] == 0)
    MatrixOS::MIDI::Send(MidiPacket::NoteOff(channel, chord.note, 0));
}

void ChordMachine::AllNotesOff() {
  for (uint8_t note = 0; note < 128; note++)
  {
    if (noteCount[note])
      MatrixOS::MIDI::Send(MidiPacket::NoteOff(channel, note, 0));
    noteCount[note] = 0;
  }
}
