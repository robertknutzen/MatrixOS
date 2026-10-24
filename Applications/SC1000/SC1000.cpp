#include "SC1000.h"

void SC1000::Setup(const vector<string>& args) {
  testPadHeld = false;
  MatrixOS::LED::Fill(Color(0));
}

void SC1000::Loop() {
  InputEvent inputEvent;
  while (MatrixOS::Input::Get(&inputEvent))
  {
    if (inputEvent.inputClass != InputClass::Keypad)
      continue;
    InputEventHandler(inputEvent);
  }

  if (!renderTimer.Tick(1000 / Device::LED::fps))
    return;

  Render();
}

void SC1000::End() {
  // Don't leave a note hanging on the SC1000
  if (testPadHeld)
  {
    MatrixOS::MIDI::Send(MidiPacket::NoteOff(CH_BEATS, 0, 0), MIDI_PORT_USB);
    testPadHeld = false;
  }
}

void SC1000::InputEventHandler(InputEvent& inputEvent) {
  if (inputEvent.id == InputId::FunctionKey())
  {
    if (inputEvent.keypad.state == KeypadState::Hold)
      Exit();
    return;
  }

  Point xy;
  if (!MatrixOS::Input::GetPosition(inputEvent.id, &xy) || xy != Point(0, 0))
    return;

  // Milestone 2 test pad: beats deck cue 0 (ch 1, note 0)
  if (inputEvent.keypad.state == KeypadState::Pressed)
  {
    MatrixOS::MIDI::Send(MidiPacket::NoteOn(CH_BEATS, 0, 127), MIDI_PORT_USB);
    testPadHeld = true;
  }
  else if (inputEvent.keypad.state == KeypadState::Released)
  {
    MatrixOS::MIDI::Send(MidiPacket::NoteOff(CH_BEATS, 0, 0), MIDI_PORT_USB);
    testPadHeld = false;
  }
}

void SC1000::Render() {
  Color orange = info.color;
  MatrixOS::LED::SetColor(Point(0, 0), testPadHeld ? orange : orange.Dim(48));
  MatrixOS::LED::Update();
}
