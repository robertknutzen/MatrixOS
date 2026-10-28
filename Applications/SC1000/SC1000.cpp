#include "SC1000.h"
#include <cmath>

void SC1000::Setup(const vector<string>& args) {
  MatrixOS::LED::Fill(Color(0));
  uint64_t now = MatrixOS::SYS::Micros();
  lastSendUs = now;
  clockOriginUs = now;
  launchMs = (uint32_t)MatrixOS::SYS::Millis();
  GenerateKeyMap();
}

void SC1000::Loop() {
  InputEvent inputEvent;
  while (MatrixOS::Input::Get(&inputEvent))
  {
    if (inputEvent.inputClass != InputClass::Keypad)
      continue;
    InputEventHandler(inputEvent);
  }

  Background();

  if (renderTimer.Tick(1000 / Device::LED::fps))
    Render();
}

void SC1000::Background() {
  HandleNudgeRepeat();
  BeatRepeatTick();
  FlushOutput();
}

void SC1000::End() {
  // Delete any scratch cue left by a beat repeat that was still running
  for (uint8_t deck = 0; deck < 2; deck++)
  {
    if (beatRepeat[deck].started)
    {
      Queue(MidiPacket::NoteOn(CH_CONTROL, NOTE_SHIFT, 127));
      Queue(MidiPacket::NoteOn(deck, CUE_BEAT_REPEAT, 127));
      Queue(MidiPacket::NoteOff(CH_CONTROL, NOTE_SHIFT, 0));
    }
  }
  while (!outQueue.empty())
  {
    SendSpaced(outQueue.front());
    outQueue.pop_front();
  }
}

// ---------- Internal clock ----------

uint32_t SC1000::BeatUs() {
  return 60000000UL / bpm;
}

uint64_t SC1000::NextBeatUs(uint64_t now) {
  uint32_t beat = BeatUs();
  uint64_t elapsed = now - clockOriginUs;
  return clockOriginUs + (elapsed / beat + 1) * beat;
}

void SC1000::SetBpm(uint16_t newBpm) {
  // Keep the current position within the beat so the pulse doesn't jump
  uint64_t now = MatrixOS::SYS::Micros();
  uint32_t oldBeat = BeatUs();
  double phase = (double)((now - clockOriginUs) % oldBeat) / oldBeat;
  bpm = newBpm;
  clockOriginUs = now - (uint64_t)(phase * BeatUs());
}

// ---------- Beat repeat ----------

uint32_t SC1000::RepeatIntervalUs(uint8_t rate) {
  return BeatUs() * 4 / rate; // rate 4 = quarter note = one beat
}

void SC1000::BeatRepeatPad(uint8_t deck, uint8_t column, KeypadState state) {
  static const uint8_t rates[4] = {4, 8, 16, 32};
  uint8_t rate = rates[column];
  uint8_t latched = latchedRate[deck];

  if (state == KeypadState::Released)
  {
    if (ignoreRelease[deck][column])
      ignoreRelease[deck][column] = false;
    else
      BeatRepeatRelease(deck, rate);
    return;
  }

  if (latched == rate)
  {
    // Pressing the latched pad again stops it
    latchedRate[deck] = 0;
    ignoreRelease[deck][column] = true;
    BeatRepeatRelease(deck, rate);
    return;
  }

  // New rate takes over first, then the old latch is dropped, so the repeat keeps its captured position
  BeatRepeatPress(deck, rate);
  if (latched)
  {
    latchedRate[deck] = 0;
    BeatRepeatRelease(deck, latched);
  }

  if (ShiftHeld())
  {
    latchedRate[deck] = rate;
    ignoreRelease[deck][column] = true;
  }
}

void SC1000::BeatRepeatPress(uint8_t deck, uint8_t rate) {
  BeatRepeat& br = beatRepeat[deck];
  br.heldRates.push_back(rate);
  uint64_t now = MatrixOS::SYS::Micros();

  if (br.started)
  {
    // Newest rate takes over from the last trigger
    br.nextRepeatUs = br.lastTriggerUs + RepeatIntervalUs(rate);
    return;
  }
  if (br.pending)
    return;

  if (brQuantize)
  {
    br.pending = true;
    br.pendingStartUs = NextBeatUs(now);
  }
  else
  {
    BeatRepeatStart(deck, now);
  }
}

void SC1000::BeatRepeatStart(uint8_t deck, uint64_t now) {
  BeatRepeat& br = beatRepeat[deck];
  br.pending = false;
  br.started = true;
  br.lastTriggerUs = now;
  br.nextRepeatUs = now + RepeatIntervalUs(br.heldRates.back());
  Queue(MidiPacket::NoteOn(deck, CUE_BEAT_REPEAT, 127)); // Empty cue: this sets it here
}

void SC1000::BeatRepeatRelease(uint8_t deck, uint8_t rate) {
  BeatRepeat& br = beatRepeat[deck];
  for (auto it = br.heldRates.begin(); it != br.heldRates.end(); ++it)
  {
    if (*it == rate)
    {
      br.heldRates.erase(it);
      break;
    }
  }

  if (!br.heldRates.empty())
  {
    if (br.started)
      br.nextRepeatUs = br.lastTriggerUs + RepeatIntervalUs(br.heldRates.back());
    return;
  }

  if (br.started)
  {
    // Delete the scratch cue so the next press captures a new position
    Queue(MidiPacket::NoteOn(CH_CONTROL, NOTE_SHIFT, 127));
    Queue(MidiPacket::NoteOn(deck, CUE_BEAT_REPEAT, 127));
    Queue(MidiPacket::NoteOff(CH_CONTROL, NOTE_SHIFT, 0));
  }
  br.started = false;
  br.pending = false;
}

void SC1000::BeatRepeatTick() {
  uint64_t now = MatrixOS::SYS::Micros();
  for (uint8_t deck = 0; deck < 2; deck++)
  {
    BeatRepeat& br = beatRepeat[deck];
    if (br.pending && now >= br.pendingStartUs)
      BeatRepeatStart(deck, br.pendingStartUs);

    if (!br.started || now < br.nextRepeatUs)
      continue;

    Queue(MidiPacket::NoteOn(deck, CUE_BEAT_REPEAT, 127));
    br.lastTriggerUs = br.nextRepeatUs;
    br.nextRepeatUs += RepeatIntervalUs(br.heldRates.back());
    if (now > br.nextRepeatUs) // Fell far behind: resync rather than burst
      br.nextRepeatUs = now + RepeatIntervalUs(br.heldRates.back());
  }
}

// ---------- Keys ----------

static uint16_t AlignScale(uint16_t scale, uint8_t root) {
  // Rotate the C-based scale mask to the key (same formula as NotePad::GenerateKeymap)
  return ((scale << root) + ((scale & 0xFFF) >> (12 - root % 12))) & 0xFFF;
}

bool SC1000::IsKeyRoot(uint8_t note) {
  return note % 12 == keyRoot.Get() % 12;
}

void SC1000::GenerateKeyMap() {
  // One octave per row, like the Note app's octave layout. Middle row starts at the key root nearest the original pitch.
  uint8_t root = keyRoot.Get() % 12;
  uint16_t aligned = AlignScale(scale.Get(), root);
  if (aligned == 0)
    aligned = 0xFFF;
  int16_t middleRoot = root <= 6 ? 60 + root : 48 + root;

  for (uint8_t row = 0; row < 3; row++)
  {
    int16_t note = middleRoot + 12 * (1 - row); // Row 0 is the top: an octave up
    for (uint8_t x = 0; x < 8; x++)
    {
      while (!bitRead(aligned, note % 12))
        note++;
      bool inRange = note >= SAMPLE_ORIGINAL_NOTE - KEY_RANGE && note <= SAMPLE_ORIGINAL_NOTE + KEY_RANGE && note <= 127;
      keyMap[row][x] = inRange ? (uint8_t)note : 255;
      note++;
    }
  }
}

// ---------- Actions ----------

bool SC1000::ShiftHeld() {
  // Main: row 8, columns 1 and 8. Perform: row 4, columns 1 and 8
  if (page == PAGE_MAIN)
    return held[0][7] || held[7][7];
  if (page == PAGE_PERFORM)
    return held[0][3] || held[7][3];
  return false;
}

void SC1000::CuePad(uint8_t deck, uint8_t cue) {
  if (ShiftHeld())
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

void SC1000::Nudge(uint8_t deck, int8_t direction) {
  SetPitch(deck, pitch[deck] + direction * PITCH_STEP);
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

void SC1000::ResetSamplePitch() {
  SetPitch(DECK_SAMPLES, 0);
  PlaySampleNote(SAMPLE_ORIGINAL_NOTE);
}

void SC1000::PlaySampleNote(uint8_t note) {
  Queue(MidiPacket::NoteOn(CH_KEYS, note, 127));
  sampleNote = note;
}

void SC1000::ChangeFile(uint8_t deck, bool next) {
  // Shift moves between folders instead of files
  uint8_t note = ShiftHeld() ? (next ? NOTE_NEXT_FOLDER[deck] : NOTE_PREV_FOLDER[deck])
                             : (next ? NOTE_NEXT_FILE[deck] : NOTE_PREV_FILE[deck]);
  Queue(MidiPacket::NoteOn(CH_CONTROL, note, 127));
  ResetDeckMirror(deck);
}

void SC1000::ResetDeckMirror(uint8_t deck) {
  // Changing files on the SC1000 resets pitch and note, and loads cues we can't see
  pitch[deck] = 0;
  if (deck == DECK_SAMPLES)
    sampleNote = SAMPLE_ORIGINAL_NOTE;
  for (bool& cue : cueSet[deck])
    cue = false;
}

int8_t SC1000::SpeedDirection(uint8_t deck) {
  double speed = 1.0 + pitch[deck] / 10000.0;
  if (deck == DECK_SAMPLES)
    speed *= std::pow(2.0, (sampleNote - SAMPLE_ORIGINAL_NOTE) / 12.0);
  if (std::fabs(speed - 1.0) < 1e-9)
    return 0;
  return speed < 1.0 ? -1 : 1;
}

void SC1000::HandleNudgeRepeat() {
  if (page != PAGE_MAIN)
    return;
  // Main row 6 (y = 5): SP- x0, SP+ x2, BP- x5, BP+ x7
  static const uint8_t nudgePads[4] = {0, 2, 5, 7};
  uint32_t now = (uint32_t)MatrixOS::SYS::Millis();
  for (uint8_t x : nudgePads)
  {
    if (!held[x][5])
      continue;
    if (now - holdStartMs[x][5] < NUDGE_REPEAT_DELAY_MS || now - lastRepeatMs[x][5] < NUDGE_REPEAT_INTERVAL_MS)
      continue;
    lastRepeatMs[x][5] = now;
    Nudge(x < 4 ? DECK_SAMPLES : DECK_BEATS, (x == 0 || x == 5) ? -1 : 1);
  }
}

// ---------- Output ----------

void SC1000::Queue(MidiPacket packet) {
  outQueue.push_back(packet);
}

void SC1000::FlushOutput() {
  uint64_t now = MatrixOS::SYS::Micros();
  if (outQueue.empty() || now - lastSendUs < MIN_SEND_GAP_US)
    return;
  MatrixOS::MIDI::Send(outQueue.front(), MIDI_PORT_USB);
  outQueue.pop_front();
  lastSendUs = now;
}

void SC1000::SendSpaced(MidiPacket packet) {
  uint64_t elapsed = MatrixOS::SYS::Micros() - lastSendUs;
  if (elapsed < MIN_SEND_GAP_US)
    MatrixOS::SYS::DelayMs((MIN_SEND_GAP_US - elapsed) / 1000 + 1);
  MatrixOS::MIDI::Send(packet, MIDI_PORT_USB);
  lastSendUs = MatrixOS::SYS::Micros();
}
