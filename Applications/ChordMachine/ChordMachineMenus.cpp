// Settings menu (function key tap) and the scale page, built from the Note app's menu components.
#include "ChordMachine.h"
#include "UI/UI.h"
#include "ScaleVisualizer.h" // From Applications/Note

namespace
{
const Color COLOR_CHANNEL = Color(0x60FF00);
const Color COLOR_SCALE = Color(0xFFE000);
const Color COLOR_NRPN = Color(0x00C0FF);

uint16_t scaleList[16] = {MAJOR,           MINOR,          DORIAN,           PHRYGIAN,   LYDIAN,           MIXOLYDIAN,
                          LOCRIAN,         HARMONIC_MINOR, MELODIC_MINOR_ASCENDING, BLUES, MAJOR_PENTATONIC, MINOR_PENTATONIC,
                          HUNGARIAN_MINOR, WHOLE_TONE,     MAJOR_BLUES,      CHROMATIC};
const string scaleNames[16] = {"Major",           "Minor",          "Dorian",           "Phrygian",   "Lydian",           "Mixolydian",
                               "Locrian",         "Harmonic Minor", "Melodic Minor Ascending", "Blues", "Major Pentatonic", "Minor Pentatonic",
                               "Hungarian Minor", "Whole Tone",     "Major Blues",      "Chromatic"};

// Tap the function key to go back, hold it to leave the app
template <typename ExitApp> bool FunctionKeyHandler(UI& page, InputEvent* inputEvent, ExitApp exitApp) {
  if (inputEvent->id != InputId::FunctionKey())
    return false;
  if (inputEvent->keypad.state == KeypadState::Hold)
    exitApp();
  else if (inputEvent->keypad.state == KeypadState::Released && !inputEvent->keypad.hold)
    page.Exit();
  return true;
}
} // namespace

void ChordMachine::SettingsMenu() {
  AllNotesOff(); // Safety net: the play screen already released its pads

  UI settings("Settings", COLOR_SCALE, false);

  // Rows 1-2: MIDI channel 1-16 (the Syntakt track's channel)
  uint16_t channelValue = channel;
  UISelector channelSelector;
  channelSelector.SetDimension(Dimension(8, 2));
  channelSelector.SetName("MIDI Channel");
  channelSelector.SetColor(COLOR_CHANNEL);
  channelSelector.SetCount(16);
  channelSelector.SetValuePointer(&channelValue);
  channelSelector.OnChange([&](uint16_t value) -> void { channel = (uint8_t)value; });
  settings.AddUIComponent(channelSelector, Point(0, 0));

  // Row 8, col 1: scale and key
  UIButton scaleButton;
  scaleButton.SetName("Scale");
  scaleButton.SetColor(COLOR_SCALE);
  scaleButton.OnPress([&]() -> void { ScalePage(); });
  settings.AddUIComponent(scaleButton, Point(0, 7));

  // Row 8, col 8: send TYPE and BAL as NRPN (lit) or CC (dim)
  bool nrpnValue = useNrpn;
  UIToggle nrpnToggle;
  nrpnToggle.SetName("NRPN");
  nrpnToggle.SetColor(COLOR_NRPN);
  nrpnToggle.SetValuePointer(&nrpnValue);
  nrpnToggle.OnPress([&]() -> void { useNrpn = nrpnValue; });
  settings.AddUIComponent(nrpnToggle, Point(7, 7));

  settings.SetInputEventHandler([&](InputEvent* inputEvent) -> bool {
    return FunctionKeyHandler(settings, inputEvent, [&]() { Exit(); });
  });

  settings.Start();
}

void ChordMachine::ScalePage() {
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

  scalePage.SetInputEventHandler([&](InputEvent* inputEvent) -> bool {
    return FunctionKeyHandler(scalePage, inputEvent, [&]() { Exit(); });
  });

  scalePage.Start();

  if (root != keyRoot)
    keyRoot = root;
  if (scaleValue != scale)
    scale = scaleValue;
  UpdateScale();
}
