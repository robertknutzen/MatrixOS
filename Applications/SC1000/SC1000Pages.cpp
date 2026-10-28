// Input and LEDs for the Main, Settings and Perform pages.
// Coordinates are 0-based (x = column, y = row from the top); the user's tables are 1-based.
#include "SC1000.h"

namespace
{
const Color COLOR_SAMPLES = Color(0x0050FF);
const Color COLOR_BEATS = Color(0x00FF30);
const Color COLOR_SAMPLES_REPEAT = Color(0x4A20FF);
const Color COLOR_BEATS_REPEAT = Color(0x70FF00);
const Color COLOR_SHIFT = Color(0xFF3090);
const Color COLOR_DELETE = Color(0xFF0000);
const Color COLOR_EXIT = Color(0xFF0000);
const Color COLOR_BPM = Color(0xFF8000);
const Color COLOR_SCALE = Color(0xFFE000);
const Color COLOR_QUANTIZE = Color(0x8000FF);
const Color COLOR_KEYS = Color(0xFFFFFF);
const Color COLOR_PERFORM = Color(0x8000FF);
const Color COLOR_BACK = Color(0x00FF30);
const Color COLOR_ORIGINAL_PITCH = Color(0xFF0000);

const uint8_t DIM = 40;     // Idle pads
const uint8_t BRIGHT = 150; // "Lit" pads; held pads go to full brightness
const uint8_t REPEAT_RATES[4] = {4, 8, 16, 32};
} // namespace

// ---------- Input ----------

void SC1000::InputEventHandler(InputEvent& inputEvent) {
  if (inputEvent.id == InputId::FunctionKey())
  {
    if (inputEvent.keypad.state == KeypadState::Hold)
      Exit();
    else if (inputEvent.keypad.state == KeypadState::Released && !inputEvent.keypad.hold)
      page = page == PAGE_MAIN ? PAGE_SETTINGS : PAGE_MAIN; // Tap: Main opens Settings, other pages go back to Main
    return;
  }

  Point xy;
  if (!MatrixOS::Input::GetPosition(inputEvent.id, &xy))
    return;
  if (xy.x < 0 || xy.x >= 8 || xy.y < 0 || xy.y >= 8)
    return;

  PadEvent(xy, inputEvent.keypad.state);
}

void SC1000::PadEvent(Point xy, KeypadState state) {
  if (state == KeypadState::Pressed && (uint32_t)MatrixOS::SYS::Millis() - launchMs < LAUNCH_IGNORE_MS)
    return; // Leftover bounce from the launcher icon; its release is ignored too since held stays false

  if (state == KeypadState::Released && !held[xy.x][xy.y])
    return;

  if (state == KeypadState::Pressed)
  {
    held[xy.x][xy.y] = true;
    holdStartMs[xy.x][xy.y] = (uint32_t)MatrixOS::SYS::Millis();
    lastRepeatMs[xy.x][xy.y] = holdStartMs[xy.x][xy.y];
  }
  else if (state == KeypadState::Released)
  {
    held[xy.x][xy.y] = false;
  }
  else
  {
    return; // Hold / aftertouch aren't used
  }

  // A beat repeat pad released after switching to Settings must still stop the repeat
  if (page == PAGE_SETTINGS && state == KeypadState::Released && xy.y == 2)
  {
    TopRowsEvent(xy, state);
    return;
  }

  switch (page)
  {
  case PAGE_MAIN: MainEvent(xy, state); break;
  case PAGE_SETTINGS: SettingsEvent(xy, state); break;
  case PAGE_PERFORM: PerformEvent(xy, state); break;
  }
}

// Rows 1-3, shared by Main and Perform: cues and beat repeat
bool SC1000::TopRowsEvent(Point xy, KeypadState state) {
  if (xy.y > 2)
    return false;

  uint8_t deck = xy.x < 4 ? DECK_SAMPLES : DECK_BEATS;
  uint8_t column = xy.x % 4;

  if (xy.y < 2)
  {
    if (state == KeypadState::Pressed)
      CuePad(deck, xy.y * 4 + column);
  }
  else
  {
    if (state == KeypadState::Pressed)
      BeatRepeatPress(deck, REPEAT_RATES[column]);
    else
      BeatRepeatRelease(deck, REPEAT_RATES[column]);
  }
  return true;
}

void SC1000::MainEvent(Point xy, KeypadState state) {
  if (TopRowsEvent(xy, state) || state != KeypadState::Pressed)
    return;

  if (xy.y == 4)
  {
    switch (xy.x)
    {
    case 0: ChangeFile(DECK_SAMPLES, false); break;
    case 2: ChangeFile(DECK_SAMPLES, true); break;
    case 5: ChangeFile(DECK_BEATS, false); break;
    case 7: ChangeFile(DECK_BEATS, true); break;
    }
  }
  else if (xy.y == 5)
  {
    switch (xy.x)
    {
    case 0: Nudge(DECK_SAMPLES, -1); break;
    case 1: ResetSamplePitch(); break;
    case 2: Nudge(DECK_SAMPLES, 1); break;
    case 5: Nudge(DECK_BEATS, -1); break;
    case 6: SetPitch(DECK_BEATS, 0); break;
    case 7: Nudge(DECK_BEATS, 1); break;
    }
  }
  else if (xy.y == 7)
  {
    if (xy.x == 5)
      page = PAGE_PERFORM;
  }
}

bool SC1000::IsExitPad(Point xy) {
  // An X in columns 5-8, rows 1-4
  if (xy.x < 4 || xy.y > 3)
    return false;
  uint8_t cx = xy.x - 4;
  return cx == xy.y || cx == 3 - xy.y;
}

void SC1000::SettingsEvent(Point xy, KeypadState state) {
  if (state != KeypadState::Pressed)
    return;

  if (IsExitPad(xy))
  {
    Exit();
  }
  else if (xy == Point(0, 0))
  {
    page = PAGE_MAIN;
  }
  else if (xy == Point(0, 7))
  {
    held[xy.x][xy.y] = false; // The menu takes over input until it closes
    ScalePage();
  }
  else if (xy == Point(2, 7))
  {
    brQuantize = !brQuantize;
  }
  else if (xy == Point(7, 7))
  {
    held[xy.x][xy.y] = false;
    BpmPage();
  }
}

void SC1000::PerformEvent(Point xy, KeypadState state) {
  if (TopRowsEvent(xy, state) || state != KeypadState::Pressed)
    return;

  if (xy == Point(0, 4))
  {
    PlaySampleNote(SAMPLE_ORIGINAL_NOTE);
  }
  else if (xy.y >= 5)
  {
    uint8_t note = keyMap[xy.y - 5][xy.x];
    if (note == 255)
      return;
    if (pitch[DECK_SAMPLES] != 0)
      SetPitch(DECK_SAMPLES, 0); // Tuning only holds with the pitch bend at 0%
    PlaySampleNote(note);
  }
}

// ---------- LEDs ----------

Color SC1000::Pad(Color color, bool bright, Point xy) {
  if (held[xy.x][xy.y])
    return color;
  return color.Dim(bright ? BRIGHT : DIM);
}

Color SC1000::Pulse(Color color, uint8_t rate, Point xy) {
  // Fades from bright to dim over each repeat interval of the internal clock
  if (held[xy.x][xy.y])
    return color;
  uint32_t interval = RepeatIntervalUs(rate);
  uint32_t phase = (uint32_t)((MatrixOS::SYS::Micros() - clockOriginUs) % interval);
  uint8_t level = BRIGHT - (uint8_t)((uint32_t)(BRIGHT - DIM) * phase / interval);
  return color.Dim(level);
}

void SC1000::Render() {
  MatrixOS::LED::Fill(Color(0));
  switch (page)
  {
  case PAGE_MAIN: RenderMain(); break;
  case PAGE_SETTINGS: RenderSettings(); break;
  case PAGE_PERFORM: RenderPerform(); break;
  }
  MatrixOS::LED::Update();
}

void SC1000::RenderTopRows() {
  bool shift = ShiftHeld();
  for (uint8_t x = 0; x < 8; x++)
  {
    uint8_t deck = x < 4 ? DECK_SAMPLES : DECK_BEATS;
    uint8_t column = x % 4;
    Color deckColor = deck == DECK_SAMPLES ? COLOR_SAMPLES : COLOR_BEATS;

    for (uint8_t y = 0; y < 2; y++)
    {
      Point xy(x, y);
      bool set = cueSet[deck][y * 4 + column];
      MatrixOS::LED::SetColor(xy, Pad(shift ? COLOR_DELETE : deckColor, set, xy));
    }

    Point repeat(x, 2);
    Color repeatColor = deck == DECK_SAMPLES ? COLOR_SAMPLES_REPEAT : COLOR_BEATS_REPEAT;
    MatrixOS::LED::SetColor(repeat, Pulse(repeatColor, REPEAT_RATES[column], repeat));
  }
}

void SC1000::RenderMain() {
  RenderTopRows();

  // File navigation (samples left, beats right); brighter while shift selects folders instead
  bool folders = ShiftHeld();
  MatrixOS::LED::SetColor(Point(0, 4), Pad(COLOR_SAMPLES, folders, Point(0, 4)));
  MatrixOS::LED::SetColor(Point(2, 4), Pad(COLOR_SAMPLES, folders, Point(2, 4)));
  MatrixOS::LED::SetColor(Point(5, 4), Pad(COLOR_BEATS, folders, Point(5, 4)));
  MatrixOS::LED::SetColor(Point(7, 4), Pad(COLOR_BEATS, folders, Point(7, 4)));

  // Pitch: the brighter pad of each trio shows whether the deck plays slower, normal or faster
  for (uint8_t deck = 0; deck < 2; deck++)
  {
    uint8_t base = deck == DECK_SAMPLES ? 0 : 5;
    Color deckColor = deck == DECK_SAMPLES ? COLOR_SAMPLES : COLOR_BEATS;
    int8_t direction = SpeedDirection(deck);
    for (int8_t i = 0; i < 3; i++)
    {
      Point xy(base + i, 5);
      MatrixOS::LED::SetColor(xy, Pad(deckColor, direction == i - 1, xy));
    }
  }

  // Bottom row: shift, perform (the function key opens Settings)
  bool shift = ShiftHeld();
  MatrixOS::LED::SetColor(Point(0, 7), Pad(COLOR_SHIFT, shift, Point(0, 7)));
  MatrixOS::LED::SetColor(Point(7, 7), Pad(COLOR_SHIFT, shift, Point(7, 7)));
  MatrixOS::LED::SetColor(Point(5, 7), Pad(COLOR_PERFORM, true, Point(5, 7)));
}

void SC1000::RenderSettings() {
  MatrixOS::LED::SetColor(Point(0, 0), Pad(COLOR_BACK, true, Point(0, 0)));
  for (uint8_t x = 4; x < 8; x++)
  {
    for (uint8_t y = 0; y < 4; y++)
    {
      Point xy(x, y);
      if (IsExitPad(xy))
        MatrixOS::LED::SetColor(xy, Pad(COLOR_EXIT, true, xy));
    }
  }
  MatrixOS::LED::SetColor(Point(0, 7), Pad(COLOR_SCALE, true, Point(0, 7)));
  MatrixOS::LED::SetColor(Point(2, 7), brQuantize ? COLOR_QUANTIZE : COLOR_QUANTIZE.Dim(DIM));
  MatrixOS::LED::SetColor(Point(7, 7), Pad(COLOR_BPM, true, Point(7, 7)));
}

void SC1000::RenderPerform() {
  RenderTopRows();

  MatrixOS::LED::SetColor(Point(0, 4), Pad(COLOR_ORIGINAL_PITCH, sampleNote == SAMPLE_ORIGINAL_NOTE, Point(0, 4)));

  // Keys: roots brighter so octaves are easy to see; the note currently playing is full white
  for (uint8_t row = 0; row < 3; row++)
  {
    for (uint8_t x = 0; x < 8; x++)
    {
      Point xy(x, row + 5);
      uint8_t note = keyMap[row][x];
      if (note == 255)
        continue;
      if (note == sampleNote || held[x][row + 5])
        MatrixOS::LED::SetColor(xy, COLOR_KEYS);
      else
        MatrixOS::LED::SetColor(xy, COLOR_KEYS.Dim(IsKeyRoot(note) ? BRIGHT : DIM));
    }
  }
}
