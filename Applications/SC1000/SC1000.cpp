#include "SC1000.h"

namespace
{
const Color COLOR_BEATS = Color(0xFF6000);
const Color COLOR_SAMPLES = Color(0x00B0FF);
const Color COLOR_CONTROL = Color(0xFFFFFF);
const Color COLOR_SHIFT = Color(0xA000FF);
const Color COLOR_RECORD = Color(0xFF0000);
const Color COLOR_GATE = Color(0x00FF40);
const Color COLOR_KILL = Color(0xFF2000);
const Color COLOR_DETUNED = Color(0xFFB000);
const Color COLOR_PRESS = Color(0xFFFFFF);
const uint8_t DIM = 40;
} // namespace

void SC1000::Setup(const vector<string>& args) {
  MatrixOS::LED::Fill(Color(0));
  lastSendUs = (uint32_t)MatrixOS::SYS::Micros();
}

void SC1000::Loop() {
  InputEvent inputEvent;
  while (MatrixOS::Input::Get(&inputEvent))
  {
    if (inputEvent.inputClass != InputClass::Keypad)
      continue;
    InputEventHandler(inputEvent);
  }

  HandleNudgeRepeat();
  FlushOutput();

  if (renderTimer.Tick(1000 / Device::LED::fps))
    Render();
}

void SC1000::End() {
  // Finish anything queued (e.g. a pending shift-off), then make sure no deck is left silenced by a cut pad
  while (!outQueue.empty())
  {
    SendSpaced(outQueue.front());
    outQueue.pop_front();
  }
  for (uint8_t deck = 0; deck < 2; deck++)
  {
    if (sentVolume[deck] >= 0 && sentVolume[deck] != volume[deck])
    {
      SendSpaced(MidiPacket::ControlChange(deck, 0, volume[deck]));
      sentVolume[deck] = volume[deck];
    }
  }
}

// ---------- Input ----------

void SC1000::InputEventHandler(InputEvent& inputEvent) {
  if (inputEvent.id == InputId::FunctionKey())
  {
    if (inputEvent.keypad.state == KeypadState::Hold)
      Exit();
    return;
  }

  Point xy;
  if (!MatrixOS::Input::GetPosition(inputEvent.id, &xy))
    return;
  if (xy.x < 0 || xy.x >= 8 || xy.y < 0 || xy.y >= 8)
    return;

  PadEvent(xy, inputEvent.keypad);
}

void SC1000::PadEvent(Point xy, KeypadInfo& keypad) {
  KeypadState state = keypad.state;
  if (state == KeypadState::Pressed)
    held[xy.x][xy.y] = true;
  else if (state == KeypadState::Released)
    held[xy.x][xy.y] = false;

  switch (xy.y)
  {
  case ROW_SAMPLE_CUES:
  case ROW_SAMPLE_CUES + 1:
    CuePad(DECK_SAMPLES, (xy.y - ROW_SAMPLE_CUES) * 8 + xy.x, state);
    break;
  case ROW_BEAT_CUES:
    CuePad(DECK_BEATS, xy.x, state);
    break;
  case ROW_CUT:
    CutPad(xy.x, keypad);
    break;
  case ROW_TRANSPORT:
    TransportPad(xy.x, state);
    break;
  case ROW_PITCH:
    PitchPad(xy.x, state);
    break;
  case ROW_BEATS_VOLUME:
    VolumePad(DECK_BEATS, xy.x, state);
    break;
  case ROW_SAMPLES_VOLUME:
    VolumePad(DECK_SAMPLES, xy.x, state);
    break;
  }
}

bool ShiftHeld(bool held[8][8]) {
  return held[7][SC1000::ROW_TRANSPORT];
}

void SC1000::CuePad(uint8_t deck, uint8_t cue, KeypadState state) {
  if (state != KeypadState::Pressed)
    return; // The SC1000 ignores cue note-offs

  if (ShiftHeld(held))
  {
    // Delete the cue: SC1000 shift on, cue note, shift off
    Queue(MidiPacket::NoteOn(CH_CONTROL, NOTE_SHIFT, 127));
    Queue(MidiPacket::NoteOn(deck, cue, 127));
    Queue(MidiPacket::NoteOff(CH_CONTROL, NOTE_SHIFT, 0));
    cueSet[deck][cue] = false;
    return;
  }

  // First press sets the cue, later presses jump to it
  Queue(MidiPacket::NoteOn(deck, cue, 127));
  cueSet[deck][cue] = true;
}

void SC1000::CutPad(uint8_t x, KeypadInfo& keypad) {
  if (keypad.state == KeypadState::Released)
  {
    cutPressure[x] = 0;
    if (cutMode == CUT_GATE)
      gateClosed = true;
  }
  else if (keypad.state == KeypadState::Pressed || keypad.state == KeypadState::Aftertouch)
  {
    Fract16 pressure = keypad.pressure;
    cutPressure[x] = pressure.to7bits();
    if (cutPressure[x] == 0)
      cutPressure[x] = 1; // Held pads always count as held
  }
  UpdateVolumeTargets();
}

void SC1000::TransportPad(uint8_t x, KeypadState state) {
  if (state != KeypadState::Pressed)
    return;

  switch (x)
  {
  case 0:
  case 1:
    Queue(MidiPacket::NoteOn(CH_CONTROL, NOTE_STARTSTOP[x], 127));
    break;
  case 2:
    Queue(MidiPacket::NoteOn(CH_CONTROL, NOTE_PREV_FILE[DECK_BEATS], 127));
    ResetDeckMirror(DECK_BEATS);
    break;
  case 3:
    Queue(MidiPacket::NoteOn(CH_CONTROL, NOTE_NEXT_FILE[DECK_BEATS], 127));
    ResetDeckMirror(DECK_BEATS);
    break;
  case 4:
    Queue(MidiPacket::NoteOn(CH_CONTROL, NOTE_PREV_FILE[DECK_SAMPLES], 127));
    ResetDeckMirror(DECK_SAMPLES);
    break;
  case 5:
    Queue(MidiPacket::NoteOn(CH_CONTROL, NOTE_NEXT_FILE[DECK_SAMPLES], 127));
    ResetDeckMirror(DECK_SAMPLES);
    break;
  case 6:
    if (ShiftHeld(held))
    {
      Queue(MidiPacket::NoteOn(CH_CONTROL, NOTE_RECORD, 127)); // Record sits behind shift
    }
    else
    {
      Queue(MidiPacket::NoteOn(CH_CONTROL, NOTE_RANDOM_SAMPLE, 127));
      ResetDeckMirror(DECK_SAMPLES);
    }
    break;
  case 7:
    break; // App shift: local modifier only
  }
}

void SC1000::PitchPad(uint8_t x, KeypadState state) {
  if (state == KeypadState::Pressed)
  {
    nudgeHoldStart[x] = (uint32_t)MatrixOS::SYS::Millis();
    nudgeLastRepeat[x] = nudgeHoldStart[x];
  }
  if (state != KeypadState::Pressed)
    return;

  switch (x)
  {
  case 0: Nudge(DECK_BEATS, -1); break;
  case 1: SetPitch(DECK_BEATS, 0); break;
  case 2: Nudge(DECK_BEATS, 1); break;
  case 3: coarsePitch = !coarsePitch; break;
  case 4: Nudge(DECK_SAMPLES, -1); break;
  case 5: SetPitch(DECK_SAMPLES, 0); break;
  case 6: Nudge(DECK_SAMPLES, 1); break;
  case 7: SetCutMode(cutMode == CUT_GATE ? CUT_KILL : CUT_GATE); break;
  }
}

void SC1000::HandleNudgeRepeat() {
  static const uint8_t nudgePads[4] = {0, 2, 4, 6};
  uint32_t now = (uint32_t)MatrixOS::SYS::Millis();
  for (uint8_t x : nudgePads)
  {
    if (!held[x][ROW_PITCH])
      continue;
    if (now - nudgeHoldStart[x] < NUDGE_REPEAT_DELAY_MS || now - nudgeLastRepeat[x] < NUDGE_REPEAT_INTERVAL_MS)
      continue;
    nudgeLastRepeat[x] = now;
    Nudge(x < 4 ? DECK_BEATS : DECK_SAMPLES, (x == 0 || x == 4) ? -1 : 1);
  }
}

void SC1000::VolumePad(uint8_t deck, uint8_t x, KeypadState state) {
  if (state != KeypadState::Pressed)
    return;
  volume[deck] = FaderLevel(x);
  volumeKnown[deck] = true;
  UpdateVolumeTargets();
}

// ---------- State ----------

void SC1000::Nudge(uint8_t deck, int8_t direction) {
  SetPitch(deck, pitch[deck] + direction * (coarsePitch ? PITCH_STEP_COARSE : PITCH_STEP_FINE));
}

void SC1000::SetPitch(uint8_t deck, int32_t value) {
  if (value > PITCH_RANGE)
    value = PITCH_RANGE;
  if (value < -PITCH_RANGE)
    value = -PITCH_RANGE;
  pitch[deck] = value;

  // 14-bit pitch bend, centre 8192 exactly at 0%
  int32_t bend = value >= 0 ? 8192 + value * 8191 / PITCH_RANGE : 8192 + value * 8192 / PITCH_RANGE;
  Queue(MidiPacket::PitchBend(deck, (uint16_t)bend));
}

void SC1000::ResetDeckMirror(uint8_t deck) {
  // Changing files on the SC1000 resets pitch, and loads cues we can't see
  pitch[deck] = 0;
  for (bool& cue : cueSet[deck])
    cue = false;
}

void SC1000::SetCutMode(uint8_t mode) {
  cutMode = mode;
  gateClosed = false; // Switching modes restores the stored level
  UpdateVolumeTargets();
}

void SC1000::UpdateVolumeTargets() {
  uint8_t target[2] = {volume[DECK_BEATS], volume[DECK_SAMPLES]};

  uint8_t maxPressure = 0;
  for (uint8_t p : cutPressure)
    maxPressure = p > maxPressure ? p : maxPressure;

  if (maxPressure > 0)
  {
    if (cutMode == CUT_KILL)
      target[DECK_SAMPLES] = 0;
    else
      target[DECK_SAMPLES] = pressureEnabled ? (uint8_t)((uint16_t)maxPressure * volume[DECK_SAMPLES] / 127) : volume[DECK_SAMPLES];
  }
  else if (cutMode == CUT_GATE && gateClosed)
  {
    target[DECK_SAMPLES] = 0;
  }

  // Only drive a deck's volume once the user has touched its fader or cut pads
  bool touched[2] = {volumeKnown[DECK_BEATS], volumeKnown[DECK_SAMPLES] || maxPressure > 0 || gateClosed || sentVolume[DECK_SAMPLES] >= 0};

  for (uint8_t deck = 0; deck < 2; deck++)
  {
    if (!touched[deck])
      continue;
    targetVolume[deck] = target[deck];
    volumeDirty[deck] = sentVolume[deck] != target[deck];
  }
}

// ---------- Output ----------

void SC1000::Queue(MidiPacket packet) {
  outQueue.push_back(packet);
}

void SC1000::FlushOutput() {
  uint32_t now = (uint32_t)MatrixOS::SYS::Micros();
  if (now - lastSendUs < MIN_SEND_GAP_US)
    return;

  if (!outQueue.empty())
  {
    MatrixOS::MIDI::Send(outQueue.front(), MIDI_PORT_USB);
    outQueue.pop_front();
    lastSendUs = now;
    return;
  }

  // Volume is sent as "latest value wins", so pressure changes never pile up
  for (uint8_t i = 0; i < 2; i++)
  {
    uint8_t deck = (nextVolumeDeck + i) % 2;
    if (!volumeDirty[deck])
      continue;
    MatrixOS::MIDI::Send(MidiPacket::ControlChange(deck, 0, targetVolume[deck]), MIDI_PORT_USB);
    sentVolume[deck] = targetVolume[deck];
    volumeDirty[deck] = false;
    nextVolumeDeck = (deck + 1) % 2;
    lastSendUs = now;
    return;
  }
}

void SC1000::SendSpaced(MidiPacket packet) {
  uint32_t elapsed = (uint32_t)MatrixOS::SYS::Micros() - lastSendUs;
  if (elapsed < MIN_SEND_GAP_US)
    MatrixOS::SYS::DelayMs((MIN_SEND_GAP_US - elapsed) / 1000 + 1);
  MatrixOS::MIDI::Send(packet, MIDI_PORT_USB);
  lastSendUs = (uint32_t)MatrixOS::SYS::Micros();
}

// ---------- LEDs ----------

uint8_t SC1000::FaderLevel(uint8_t x) {
  return (uint8_t)((uint16_t)x * 127 / 7); // 0, 18, 36 ... 127
}

void SC1000::Render() {
  bool shift = ShiftHeld(held);

  for (uint8_t x = 0; x < 8; x++)
  {
    // Cue pads: dim = unknown, bright = set this session
    for (uint8_t row = 0; row < 2; row++)
    {
      uint8_t cue = row * 8 + x;
      Color c = cueSet[DECK_SAMPLES][cue] ? COLOR_SAMPLES : COLOR_SAMPLES.Dim(DIM);
      if (shift)
        c = cueSet[DECK_SAMPLES][cue] ? COLOR_RECORD : COLOR_RECORD.Dim(DIM);
      MatrixOS::LED::SetColor(Point(x, ROW_SAMPLE_CUES + row), held[x][ROW_SAMPLE_CUES + row] ? COLOR_PRESS : c);
    }
    Color beatCue = cueSet[DECK_BEATS][x] ? COLOR_BEATS : COLOR_BEATS.Dim(DIM);
    if (shift)
      beatCue = cueSet[DECK_BEATS][x] ? COLOR_RECORD : COLOR_RECORD.Dim(DIM);
    MatrixOS::LED::SetColor(Point(x, ROW_BEAT_CUES), held[x][ROW_BEAT_CUES] ? COLOR_PRESS : beatCue);

    // Cut pads: brightness follows pressure while held
    Color cut = cutMode == CUT_GATE ? COLOR_GATE : COLOR_KILL;
    MatrixOS::LED::SetColor(Point(x, ROW_CUT), cutPressure[x] ? cut : cut.Dim(DIM));

    // Volume faders
    for (uint8_t deck = 0; deck < 2; deck++)
    {
      uint8_t row = deck == DECK_BEATS ? ROW_BEATS_VOLUME : ROW_SAMPLES_VOLUME;
      Color deckColor = deck == DECK_BEATS ? COLOR_BEATS : COLOR_SAMPLES;
      bool lit = volumeKnown[deck] && FaderLevel(x) <= volume[deck] && (x > 0 || volume[deck] == 0);
      MatrixOS::LED::SetColor(Point(x, row), lit ? deckColor : deckColor.Dim(volumeKnown[deck] ? 16 : DIM));
    }
  }

  // Transport
  MatrixOS::LED::SetColor(Point(0, ROW_TRANSPORT), COLOR_BEATS);
  MatrixOS::LED::SetColor(Point(1, ROW_TRANSPORT), COLOR_SAMPLES);
  MatrixOS::LED::SetColor(Point(2, ROW_TRANSPORT), COLOR_BEATS.Dim(DIM));
  MatrixOS::LED::SetColor(Point(3, ROW_TRANSPORT), COLOR_BEATS.Dim(DIM));
  MatrixOS::LED::SetColor(Point(4, ROW_TRANSPORT), COLOR_SAMPLES.Dim(DIM));
  MatrixOS::LED::SetColor(Point(5, ROW_TRANSPORT), COLOR_SAMPLES.Dim(DIM));
  MatrixOS::LED::SetColor(Point(6, ROW_TRANSPORT), shift ? COLOR_RECORD : COLOR_CONTROL.Dim(DIM));
  MatrixOS::LED::SetColor(Point(7, ROW_TRANSPORT), shift ? COLOR_SHIFT : COLOR_SHIFT.Dim(DIM));
  for (uint8_t x = 0; x < 6; x++)
  {
    if (held[x][ROW_TRANSPORT])
      MatrixOS::LED::SetColor(Point(x, ROW_TRANSPORT), COLOR_PRESS);
  }

  // Pitch: reset pads turn amber when the deck is detuned
  for (uint8_t deck = 0; deck < 2; deck++)
  {
    uint8_t base = deck == DECK_BEATS ? 0 : 4;
    Color deckColor = deck == DECK_BEATS ? COLOR_BEATS : COLOR_SAMPLES;
    MatrixOS::LED::SetColor(Point(base, ROW_PITCH), held[base][ROW_PITCH] ? COLOR_PRESS : deckColor.Dim(DIM));
    MatrixOS::LED::SetColor(Point(base + 1, ROW_PITCH), pitch[deck] == 0 ? COLOR_CONTROL.Dim(DIM) : COLOR_DETUNED);
    MatrixOS::LED::SetColor(Point(base + 2, ROW_PITCH), held[base + 2][ROW_PITCH] ? COLOR_PRESS : deckColor.Dim(DIM));
  }
  MatrixOS::LED::SetColor(Point(3, ROW_PITCH), coarsePitch ? COLOR_CONTROL : COLOR_CONTROL.Dim(DIM));
  MatrixOS::LED::SetColor(Point(7, ROW_PITCH), cutMode == CUT_GATE ? COLOR_GATE : COLOR_KILL);

  MatrixOS::LED::Update();
}
