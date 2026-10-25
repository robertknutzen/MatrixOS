// BPM and Scale pages, built from the Note app's menu components.
#include "SC1000.h"
#include "UI/UI.h"
#include "BPMTapper.h"      // From Applications/Note
#include "ScaleVisualizer.h" // From Applications/Note

namespace
{
const Color COLOR_BPM = Color(0xFF8000);
const Color COLOR_SCALE = Color(0xFFE000);

// Same tempo controls as the Note app's BPM menu
const int32_t BPM_MODIFIERS[8] = {-25, -10, -5, -1, 1, 5, 10, 25};
const uint8_t MODIFIER_GRADIENT[8] = {255, 127, 64, 32, 32, 64, 127, 255};

uint16_t scaleList[16] = {MAJOR,           MINOR,          DORIAN,           PHRYGIAN,   LYDIAN,           MIXOLYDIAN,
                          LOCRIAN,         HARMONIC_MINOR, MELODIC_MINOR_ASCENDING, BLUES, MAJOR_PENTATONIC, MINOR_PENTATONIC,
                          HUNGARIAN_MINOR, WHOLE_TONE,     MAJOR_BLUES,      CHROMATIC};
const string scaleNames[16] = {"Major",           "Minor",          "Dorian",           "Phrygian",   "Lydian",           "Mixolydian",
                               "Locrian",         "Harmonic Minor", "Melodic Minor Ascending", "Blues", "Major Pentatonic", "Minor Pentatonic",
                               "Hungarian Minor", "Whole Tone",     "Major Blues",      "Chromatic"};
} // namespace

void SC1000::BpmPage() {
  UI bpmPage("BPM", COLOR_BPM, false);
  int32_t bpmValue = bpm;

  UI4pxNumber bpmDisplay;
  bpmDisplay.SetColor(COLOR_BPM);
  bpmDisplay.SetDigits(3);
  bpmDisplay.SetValuePointer(&bpmValue);
  bpmDisplay.SetAlternativeColor(Color::White);
  bpmPage.AddUIComponent(bpmDisplay, Point(-1, 0));

  // Tapping anywhere on the number taps the tempo
  NoteBPMTapper bpmTapper;
  bpmTapper.SetName("Tap Tempo");
  bpmTapper.SetColor(COLOR_BPM);
  bpmTapper.SetSize(Dimension(8, 4));
  bpmTapper.OnChange([&](uint16_t tappedBpm) -> void {
    bpmValue = tappedBpm;
    SetBpm(tappedBpm);
  });
  bpmPage.AddUIComponent(bpmTapper, Point(0, 0));

  UINumberModifier bpmModifier;
  bpmModifier.SetColor(COLOR_BPM);
  bpmModifier.SetLength(8);
  bpmModifier.SetValuePointer(&bpmValue);
  bpmModifier.SetModifiers(BPM_MODIFIERS);
  bpmModifier.SetControlGradient(MODIFIER_GRADIENT);
  bpmModifier.SetLowerLimit(20);
  bpmModifier.SetUpperLimit(299);
  bpmModifier.OnChange([&](int32_t value) -> void { SetBpm((uint16_t)value); });
  bpmPage.AddUIComponent(bpmModifier, Point(0, 4));

  // Keep the clock, beat repeat and output running while the menu is open; flash the bottom row on each beat
  bpmPage.SetLoopFunc([&]() -> void { Background(); });
  bpmPage.SetPostRenderFunc([&]() -> void {
    uint32_t beat = BeatUs();
    uint32_t phase = (uint32_t)((MatrixOS::SYS::Micros() - clockOriginUs) % beat);
    Color light = phase < beat / 8 ? Color::White : Color::White.Dim(16);
    for (uint8_t x = 0; x < 8; x++)
      MatrixOS::LED::SetColor(Point(x, 7), light);
  });

  bpmPage.SetInputEventHandler([&](InputEvent* inputEvent) -> bool {
    if (inputEvent->id != InputId::FunctionKey())
      return false;
    if (inputEvent->keypad.state == KeypadState::Hold)
      Exit();
    else if (inputEvent->keypad.state == KeypadState::Released)
      bpmPage.Exit();
    return true;
  });

  bpmPage.Start();
}

void SC1000::ScalePage() {
  UI scalePage("Scale", COLOR_SCALE, false);
  uint8_t root = keyRoot;
  uint8_t unusedOffset = 0; // The Note app's diatonic offset isn't used here
  uint16_t scaleValue = scale;

  NoteScaleVisualizer keyPicker(&root, &unusedOffset, &scaleValue);
  scalePage.AddUIComponent(keyPicker, Point(0, 0));

  UIItemSelector<uint16_t> scaleSelector;
  scaleSelector.SetDimension(Dimension(8, 4));
  scaleSelector.SetColor(COLOR_SCALE);
  scaleSelector.SetItemPointer(&scaleValue);
  scaleSelector.SetCount(16);
  scaleSelector.SetItems(scaleList);
  scaleSelector.SetIndividualNameFunc([&](uint16_t index) -> string { return scaleNames[index]; });
  scalePage.AddUIComponent(scaleSelector, Point(0, 4));

  scalePage.SetLoopFunc([&]() -> void { Background(); });

  scalePage.SetInputEventHandler([&](InputEvent* inputEvent) -> bool {
    if (inputEvent->id != InputId::FunctionKey())
      return false;
    if (inputEvent->keypad.state == KeypadState::Hold)
      Exit();
    else if (inputEvent->keypad.state == KeypadState::Released)
      scalePage.Exit();
    return true;
  });

  scalePage.Start();

  if (root != keyRoot)
    keyRoot = root;
  if (scaleValue != scale)
    scale = scaleValue;
  GenerateKeyMap();
}
