// SC1000: one-way USB MIDI controller for the Portablism SC1000/SC500.
// Targets the default "Lemur preset" map of rasteri/SC1000 v1.7.
// Layout: the user's Main / Settings / Perform tables (see "SC1000 Controller app for Mystrix Pro — build plan.md"
// and the notes files next to it in the repo root).
#pragma once

#include <deque>
#include <vector>
#include "MatrixOS.h"
#include "Application.h"
#include "Scales.h" // From Applications/Note

class SC1000 : public Application {
public:
  inline static Application_Info info = {
      .name = "SC1000",
      .author = "b0b",
      .color = Color(0x0050FF),
      .version = 2,
      .visibility = true,
  };

  // MIDI channels used by the SC1000 default map (0-based: 0 = MIDI channel 1)
  static constexpr uint8_t CH_KEYS = 3;
  static constexpr uint8_t CH_CONTROL = 4;

  // Decks double as their MIDI channel
  static constexpr uint8_t DECK_BEATS = 0;
  static constexpr uint8_t DECK_SAMPLES = 1;

  // Channel 5 control notes, indexed by deck
  static constexpr uint8_t NOTE_NEXT_FILE[2] = {2, 3};
  static constexpr uint8_t NOTE_PREV_FILE[2] = {6, 7};
  static constexpr uint8_t NOTE_NEXT_FOLDER[2] = {4, 5};
  static constexpr uint8_t NOTE_PREV_FOLDER[2] = {8, 9};
  static constexpr uint8_t NOTE_SHIFT = 127;

  static constexpr uint8_t CUE_BEAT_REPEAT = 65; // Scratch cue, never set by the cue pads
  static constexpr uint8_t SAMPLE_ORIGINAL_NOTE = 60;
  static constexpr int8_t KEY_RANGE = 24; // Keys further than this from the original pitch are disabled

  // The SC1000 holds one pending MIDI command, so space messages out
  static constexpr uint32_t MIN_SEND_GAP_US = 5000;

  // The launcher opens apps on release; ignore the bounce of that pad as the finger lifts
  static constexpr uint32_t LAUNCH_IGNORE_MS = 250;

  // Pitch is tracked in hundredths of a percent; must match scsettings pitchrange
  static constexpr int32_t PITCH_RANGE = 50 * 100;
  static constexpr int32_t PITCH_STEP = 10; // 0.1%
  static constexpr uint32_t NUDGE_REPEAT_DELAY_MS = 400;
  static constexpr uint32_t NUDGE_REPEAT_INTERVAL_MS = 80;

  enum Page : uint8_t { PAGE_MAIN, PAGE_SETTINGS, PAGE_PERFORM };

  void Setup(const vector<string>& args) override;
  void Loop() override;
  void End() override;

private:
  // Settings
  CreateSavedVar("SC1000", bpm, uint16_t, 120);
  CreateSavedVar("SC1000", keyRoot, uint8_t, 0);
  CreateSavedVar("SC1000", scale, uint16_t, MAJOR);
  CreateSavedVar("SC1000", brQuantize, bool, false);

  Page page = PAGE_MAIN;
  uint32_t launchMs = 0;
  Timer renderTimer;
  bool held[8][8] = {};
  uint32_t holdStartMs[8][8] = {};
  uint32_t lastRepeatMs[8][8] = {};

  // State mirror (the SC1000 never reports back)
  bool cueSet[2][8] = {};
  int32_t pitch[2] = {0, 0};
  uint8_t sampleNote = SAMPLE_ORIGINAL_NOTE;

  // Internal clock (not sent as MIDI clock)
  uint64_t clockOriginUs = 0;
  uint32_t BeatUs();
  uint64_t NextBeatUs(uint64_t now);
  void SetBpm(uint16_t newBpm);

  // Beat repeat, one engine per deck
  struct BeatRepeat {
    std::vector<uint8_t> heldRates; // Most recent last; 4/8/16/32
    bool started = false;
    bool pending = false;
    uint64_t pendingStartUs = 0;
    uint64_t lastTriggerUs = 0;
    uint64_t nextRepeatUs = 0;
  };
  BeatRepeat beatRepeat[2];
  uint8_t latchedRate[2] = {0, 0};        // Rate latched with shift, 0 = none
  bool ignoreRelease[2][4] = {};          // Pad press was a latch toggle; its release does nothing
  void BeatRepeatPad(uint8_t deck, uint8_t column, KeypadState state);
  uint32_t RepeatIntervalUs(uint8_t rate);
  void BeatRepeatPress(uint8_t deck, uint8_t rate);
  void BeatRepeatRelease(uint8_t deck, uint8_t rate);
  void BeatRepeatTick();
  void BeatRepeatStart(uint8_t deck, uint64_t now);

  // Perform keys: rows 5-7 (0-based), 255 = disabled
  uint8_t keyMap[3][8] = {};
  void GenerateKeyMap();
  bool IsKeyRoot(uint8_t note);

  // Output
  std::deque<MidiPacket> outQueue;
  uint64_t lastSendUs = 0;
  void Queue(MidiPacket packet);
  void FlushOutput();
  void SendSpaced(MidiPacket packet);

  // Shared actions
  void Background(); // Clock-driven work that must keep running on every page and menu
  void CuePad(uint8_t deck, uint8_t cue);
  void Nudge(uint8_t deck, int8_t direction);
  void SetPitch(uint8_t deck, int32_t value);
  void ResetSamplePitch();
  void PlaySampleNote(uint8_t note);
  void ChangeFile(uint8_t deck, bool next);
  void ResetDeckMirror(uint8_t deck);
  int8_t SpeedDirection(uint8_t deck); // -1 slower, 0 normal, 1 faster
  bool ShiftHeld();
  void HandleNudgeRepeat();

  // Pages (SC1000Pages.cpp)
  void InputEventHandler(InputEvent& inputEvent);
  void PadEvent(Point xy, KeypadState state);
  bool TopRowsEvent(Point xy, KeypadState state);
  void MainEvent(Point xy, KeypadState state);
  void SettingsEvent(Point xy, KeypadState state);
  void PerformEvent(Point xy, KeypadState state);
  void Render();
  void RenderTopRows();
  void RenderMain();
  void RenderSettings();
  void RenderPerform();
  Color Pad(Color color, bool bright, Point xy);
  Color Pulse(Color color, uint8_t rate, Point xy);
  static bool IsExitPad(Point xy);

  // Menus (SC1000Menus.cpp)
  void BpmPage();
  void ScalePage();
};
