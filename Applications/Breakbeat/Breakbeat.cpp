#include "Breakbeat.h"

// Coordinates are 0-based (x = column, y = row from the top). The README uses 1-based rows/columns.

namespace
{
// Same colours as the sampler's touchscreen
const Color COLOR_SLICE = Color(0x3050FF);
const Color COLOR_PLAYHEAD = Color(0xFF9000);
const Color COLOR_HELD = Color(0xFF2060);
const Color COLOR_QUEUED = Color(0xFFE000);
const Color COLOR_OFFLINE = Color(0xFFFFFF);

const Color COLOR_SLICE_RES = Color(0x00E0FF);
const Color COLOR_BARS = Color(0x3050FF);
const Color COLOR_DIRECTION = Color(0x00FF40);
const Color COLOR_SLICE_FX = Color(0xA020FF);
const Color COLOR_TOGGLE = Color(0xFF9000);
const Color COLOR_PANIC = Color(0xFF0000);
const Color COLOR_LP = Color(0x3080FF);
const Color COLOR_HP = Color(0xFF2060);
const Color COLOR_FILTER_OFF = Color(0xFFFFFF);
const Color COLOR_CRUSH = Color(0xFFB000);
const Color COLOR_REC = Color(0xFF0000);
const Color COLOR_REC_ARMED = Color(0xFFE000);
const Color COLOR_UNDO = Color(0x00FF40);
const Color COLOR_LOAD = Color(0x3050FF);
const Color COLOR_PLAY = Color(0x00FF40);

const uint8_t DIM = 30;     // Unselected options, idle pads
const uint8_t BRIGHT = 150; // Selected options

// DJ filter row: low-pass on the left, off in the middle two pads, high-pass on the right
const uint8_t FILTER_VALUES[8] = {0, 21, 42, 64, 64, 85, 106, 127};
} // namespace

void Breakbeat::Setup(const vector<string>& args) {
  MatrixOS::LED::Fill(Color(0));
  launchMs = (uint32_t)MatrixOS::SYS::Millis();
  // Defaults until the sampler reports its settings
  cc[CC_SLICE_RES] = 127;
  cc[CC_FOLLOW] = 127;
  cc[CC_QUANTIZE] = 127;
  cc[CC_FILTER] = 64;
  RequestRefresh();
}

void Breakbeat::Loop() {
  MidiPacket packet;
  while (MatrixOS::MIDI::Get(&packet))
    HandleMidi(packet);

  InputEvent inputEvent;
  while (MatrixOS::Input::Get(&inputEvent))
  {
    if (inputEvent.inputClass == InputClass::Keypad)
      InputEventHandler(inputEvent);
  }

  uint32_t now = (uint32_t)MatrixOS::SYS::Millis();
  // Tell the sampler we're listening; it only sends feedback while this keeps coming
  if (now - lastAliveMs >= ALIVE_INTERVAL_MS)
  {
    SendCC(CC_ALIVE, 127);
    lastAliveMs = now;
  }
  // Sampler gone quiet (restarted, unplugged): show offline and ask again
  if (connected && now - lastFeedbackMs > FEEDBACK_TIMEOUT_MS)
    connected = false;
  // The sampler may start after the app: keep asking until it answers
  if (!connected && now - lastRefreshMs > REFRESH_RETRY_MS)
    RequestRefresh();

  if (renderTimer.Tick(1000 / Device::LED::fps))
    Render();
}

void Breakbeat::End() {
  // Don't leave slices stuck on the sampler, and stop its feedback right away
  ReleaseAllSlices();
  SendCC(CC_ALIVE, 0);
}

// ---------- MIDI ----------

void Breakbeat::Send(MidiPacket packet) {
  MatrixOS::MIDI::Send(packet, MIDI_PORT_USB);
}

void Breakbeat::SendCC(uint8_t controller, uint8_t value) {
  Send(MidiPacket::ControlChange(CHANNEL, controller, value));
}

void Breakbeat::RequestRefresh() {
  SendCC(CC_REFRESH, 127);
  lastRefreshMs = (uint32_t)MatrixOS::SYS::Millis();
}

void Breakbeat::HandleMidi(const MidiPacket& packet) {
  if (packet.Channel() != CHANNEL)
    return;
  switch (packet.Status())
  {
  case NoteOn:
  case NoteOff:
  {
    uint8_t note = packet.Note();
    if (note < FIRST_NOTE || note >= FIRST_NOTE + 64)
      return;
    led[note - FIRST_NOTE] = packet.Status() == NoteOn ? packet.Velocity() : LED_OFF;
    connected = true;
    lastFeedbackMs = (uint32_t)MatrixOS::SYS::Millis();
    break;
  }
  case ControlChange:
    cc[packet.Controller() & 0x7F] = (uint8_t)packet.Value();
    connected = true;
    lastFeedbackMs = (uint32_t)MatrixOS::SYS::Millis();
    break;
  default:
    break;
  }
}

void Breakbeat::ReleaseAllSlices() {
  for (uint8_t y = 0; y < 8; y++)
  {
    for (uint8_t x = 0; x < 8; x++)
    {
      if (page == PAGE_PLAY && held[x][y])
        Send(MidiPacket::NoteOff(CHANNEL, FIRST_NOTE + y * 8 + x, 0));
      held[x][y] = false;
    }
  }
}

uint8_t Breakbeat::OptionIndex(uint8_t value, uint8_t n) {
  uint8_t i = (uint16_t)value * n / 128;
  return i < n ? i : n - 1;
}

uint8_t Breakbeat::OptionValue(uint8_t index, uint8_t n) {
  // 0 ... 127 evenly spread, rounded up (matches the sampler: 0/64/127, 0/43/85/127)
  return (uint8_t)((index * 127 + (n - 2)) / (n - 1));
}

uint8_t Breakbeat::SlicesPerBeat() {
  static const uint8_t perBeat[3] = {1, 2, 4};
  return perBeat[Option(CC_SLICE_RES, 3)];
}

// ---------- Input ----------

void Breakbeat::InputEventHandler(InputEvent& inputEvent) {
  if (inputEvent.id == InputId::FunctionKey())
  {
    if (inputEvent.keypad.state == KeypadState::Hold)
      Exit();
    else if (inputEvent.keypad.state == KeypadState::Released && !inputEvent.keypad.hold)
    {
      ReleaseAllSlices(); // A slice held while switching pages would otherwise stick
      page = page == PAGE_PLAY ? PAGE_SETTINGS : PAGE_PLAY;
    }
    return;
  }

  Point xy;
  if (!MatrixOS::Input::GetPosition(inputEvent.id, &xy))
    return;
  if (xy.x < 0 || xy.x >= 8 || xy.y < 0 || xy.y >= 8)
    return;

  KeypadState state = inputEvent.keypad.state;
  if (state == KeypadState::Pressed && (uint32_t)MatrixOS::SYS::Millis() - launchMs < LAUNCH_IGNORE_MS)
    return; // Leftover bounce from the launcher icon
  if (state == KeypadState::Pressed)
    held[xy.x][xy.y] = true;
  else if (state == KeypadState::Released)
  {
    if (!held[xy.x][xy.y])
      return;
    held[xy.x][xy.y] = false;
  }
  else
    return; // Hold / aftertouch aren't used

  if (page == PAGE_PLAY)
    PlayEvent(xy, state);
  else
    SettingsEvent(xy, state);
}

void Breakbeat::PlayEvent(Point xy, KeypadState state) {
  uint8_t note = FIRST_NOTE + xy.y * 8 + xy.x;
  if (state == KeypadState::Pressed)
    Send(MidiPacket::NoteOn(CHANNEL, note, 127));
  else
    Send(MidiPacket::NoteOff(CHANNEL, note, 0));
}

void Breakbeat::SettingsEvent(Point xy, KeypadState state) {
  if (state != KeypadState::Pressed)
    return;

  // Set a CC locally too, so the pad lights at once instead of after the echo
  auto set = [&](uint8_t controller, uint8_t value) {
    cc[controller] = value;
    SendCC(controller, value);
  };

  switch (xy.y)
  {
  case 0:
    if (xy.x <= 2)
      set(CC_SLICE_RES, OptionValue(xy.x, 3));
    else if (xy.x >= 4)
      set(CC_BARS, OptionValue(xy.x - 4, 4));
    break;
  case 1:
    if (xy.x <= 2)
      set(CC_DIRECTION, OptionValue(xy.x, 3));
    else if (xy.x >= 5)
      set(CC_SLICE_FX, OptionValue(xy.x - 5, 3));
    break;
  case 2:
    if (xy.x == 0)
      set(CC_FOLLOW, cc[CC_FOLLOW] >= 64 ? 0 : 127);
    else if (xy.x == 1)
      set(CC_QUANTIZE, cc[CC_QUANTIZE] >= 64 ? 0 : 127);
    else if (xy.x == 7)
      SendCC(CC_PANIC, 127);
    break;
  case 3:
    set(CC_FILTER, FILTER_VALUES[xy.x]);
    break;
  case 4:
    set(CC_CRUSH, OptionValue(xy.x, 8));
    break;
  case 5:
    if (xy.x == 0)
      set(CC_TRANSPORT, cc[CC_TRANSPORT] >= 64 ? 0 : 127); // toggle play / stop
    break;
  case 6:
    if (xy.x <= 1)
      set(CC_REC_MODE, OptionValue(xy.x, 2));
    else if (xy.x >= 4)
      set(CC_REC_BARS, OptionValue(xy.x - 4, 4));
    break;
  case 7:
    if (xy.x == 0)
      SendCC(cc[CC_REC_STATE] == REC_IDLE ? CC_REC_ARM : CC_REC_STOP, 127);
    else if (xy.x == 2)
      SendCC(CC_UNDO, 127);
    else if (xy.x == 5)
      SendCC(CC_LOAD_PREV, 127);
    else if (xy.x == 6)
      SendCC(CC_LOAD_NEXT, 127);
    break;
  }
}

// ---------- LEDs ----------

Color Breakbeat::Lit(Color color, bool on, Point xy) {
  if (held[xy.x][xy.y])
    return color;
  return color.Dim(on ? BRIGHT : DIM);
}

void Breakbeat::Render() {
  MatrixOS::LED::Fill(Color(0));
  if (page == PAGE_PLAY)
    RenderPlay();
  else
    RenderSettings();
  MatrixOS::LED::Update();
}

void Breakbeat::RenderPlay() {
  uint8_t perBeat = SlicesPerBeat();
  for (uint8_t i = 0; i < 64; i++)
  {
    Point xy(i % 8, i / 8);
    Color c;
    if (!connected)
      c = COLOR_OFFLINE.Dim(12); // No sampler yet: all pads dimly lit so it's still playable
    else
    {
      switch (led[i])
      {
      case LED_SLICE:
        // Brighter on each beat, brighter still on each bar, so the grid reads like a bar
        c = COLOR_SLICE.Dim(i % (perBeat * 4) == 0 ? 90 : (i % perBeat == 0 ? 45 : 18));
        break;
      case LED_PLAYHEAD: c = COLOR_PLAYHEAD; break;
      case LED_HELD: c = COLOR_HELD; break;
      case LED_QUEUED: c = COLOR_QUEUED; break;
      default: c = Color(0); break;
      }
    }
    // Pressed pads light at once, before the sampler's answer arrives
    if (held[xy.x][xy.y] && led[i] != LED_HELD && led[i] != LED_QUEUED)
      c = COLOR_QUEUED.Dim(BRIGHT);
    MatrixOS::LED::SetColor(xy, c);
  }
}

void Breakbeat::RenderSettings() {
  auto option = [&](Point xy, Color color, uint8_t controller, uint8_t index, uint8_t n) {
    MatrixOS::LED::SetColor(xy, Lit(color, Option(controller, n) == index, xy));
  };

  // Row 1: slices, bars
  for (uint8_t i = 0; i < 3; i++)
    option(Point(i, 0), COLOR_SLICE_RES, CC_SLICE_RES, i, 3);
  for (uint8_t i = 0; i < 4; i++)
    option(Point(4 + i, 0), COLOR_BARS, CC_BARS, i, 4);

  // Row 2: playhead direction, held-slice FX
  for (uint8_t i = 0; i < 3; i++)
  {
    option(Point(i, 1), COLOR_DIRECTION, CC_DIRECTION, i, 3);
    option(Point(5 + i, 1), COLOR_SLICE_FX, CC_SLICE_FX, i, 3);
  }

  // Row 3: follow, quantize, panic
  MatrixOS::LED::SetColor(Point(0, 2), Lit(COLOR_TOGGLE, cc[CC_FOLLOW] >= 64, Point(0, 2)));
  MatrixOS::LED::SetColor(Point(1, 2), Lit(COLOR_TOGGLE, cc[CC_QUANTIZE] >= 64, Point(1, 2)));
  MatrixOS::LED::SetColor(Point(7, 2), Lit(COLOR_PANIC, false, Point(7, 2)));

  // Row 4: DJ filter. The pad nearest the current value is lit.
  uint8_t filter = cc[CC_FILTER];
  uint8_t nearest = 0;
  for (uint8_t x = 1; x < 8; x++)
  {
    if (abs((int)FILTER_VALUES[x] - filter) < abs((int)FILTER_VALUES[nearest] - filter))
      nearest = x;
  }
  for (uint8_t x = 0; x < 8; x++)
  {
    Point xy(x, 3);
    bool centre = x == 3 || x == 4;
    Color color = centre ? COLOR_FILTER_OFF : (x < 3 ? COLOR_LP : COLOR_HP);
    bool on = centre ? filter == 64 : x == nearest && filter != 64;
    MatrixOS::LED::SetColor(xy, Lit(color, on, xy));
  }

  // Row 5: bit crush as a bar graph
  uint8_t crush = cc[CC_CRUSH];
  for (uint8_t x = 0; x < 8; x++)
  {
    Point xy(x, 4);
    bool on = x == 0 ? crush == 0 : OptionValue(x, 8) <= crush;
    MatrixOS::LED::SetColor(xy, Lit(x == 0 ? COLOR_FILTER_OFF : COLOR_CRUSH, on, xy));
  }

  // Row 6: play / stop (bright green while playing)
  MatrixOS::LED::SetColor(Point(0, 5), Lit(COLOR_PLAY, cc[CC_TRANSPORT] >= 64, Point(0, 5)));

  // Row 7: record mode, record length (clock mode)
  for (uint8_t i = 0; i < 2; i++)
    option(Point(i, 6), COLOR_REC, CC_REC_MODE, i, 2);
  for (uint8_t i = 0; i < 4; i++)
    option(Point(4 + i, 6), COLOR_BARS, CC_REC_BARS, i, 4);

  // Row 8: record (colour shows the recorder's state), undo, prev / next file
  uint32_t ms = (uint32_t)MatrixOS::SYS::Millis();
  bool blink = (ms / 250) % 2;
  Color rec;
  switch (cc[CC_REC_STATE])
  {
  case REC_ARMED: rec = COLOR_REC_ARMED.Dim(blink ? 255 : 60); break;
  case REC_RECORDING: rec = COLOR_REC.Dim(blink ? 255 : 120); break;
  case REC_BUSY: rec = Color(0xFFFFFF).Dim(BRIGHT); break;
  default: rec = Lit(COLOR_REC, false, Point(0, 7)); break;
  }
  MatrixOS::LED::SetColor(Point(0, 7), rec);
  MatrixOS::LED::SetColor(Point(2, 7), Lit(COLOR_UNDO, true, Point(2, 7)));
  MatrixOS::LED::SetColor(Point(5, 7), Lit(COLOR_LOAD, true, Point(5, 7)));
  MatrixOS::LED::SetColor(Point(6, 7), Lit(COLOR_LOAD, true, Point(6, 7)));
}
