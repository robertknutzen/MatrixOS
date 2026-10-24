// SC1000: one-way USB MIDI controller for the Portablism SC1000/SC500.
// Spec: "SC1000 Controller app for Mystrix Pro — build plan.md" in the repo root.
// Targets the default "Lemur preset" map of rasteri/SC1000 v1.7.
#pragma once

#include <deque>
#include "MatrixOS.h"
#include "Application.h"

class SC1000 : public Application {
public:
  inline static Application_Info info = {
      .name = "SC1000",
      .author = "b0b",
      .color = Color(0xFF6000),
      .version = 1,
      .visibility = true,
  };

  // MIDI channels used by the SC1000 default map (0-based: 0 = MIDI channel 1)
  static constexpr uint8_t CH_BEATS = 0;
  static constexpr uint8_t CH_SAMPLES = 1;
  static constexpr uint8_t CH_KEYS = 3;
  static constexpr uint8_t CH_CONTROL = 4;

  // Channel 5 control notes
  static constexpr uint8_t NOTE_STARTSTOP[2] = {0, 1};
  static constexpr uint8_t NOTE_NEXT_FILE[2] = {2, 3};
  static constexpr uint8_t NOTE_PREV_FILE[2] = {6, 7};
  static constexpr uint8_t NOTE_RANDOM_SAMPLE = 11;
  static constexpr uint8_t NOTE_RECORD = 126;
  static constexpr uint8_t NOTE_SHIFT = 127;

  // Decks double as their MIDI channel
  static constexpr uint8_t DECK_BEATS = 0;
  static constexpr uint8_t DECK_SAMPLES = 1;

  // Perform page rows (0 = top)
  static constexpr uint8_t ROW_SAMPLE_CUES = 0; // and row 1
  static constexpr uint8_t ROW_BEAT_CUES = 2;
  static constexpr uint8_t ROW_CUT = 3;
  static constexpr uint8_t ROW_TRANSPORT = 4;
  static constexpr uint8_t ROW_PITCH = 5;
  static constexpr uint8_t ROW_BEATS_VOLUME = 6;
  static constexpr uint8_t ROW_SAMPLES_VOLUME = 7;

  // The SC1000 holds one pending MIDI command, so space messages out
  static constexpr uint32_t MIN_SEND_GAP_US = 5000;

  // Pitch is tracked in hundredths of a percent; must match scsettings pitchrange
  static constexpr int32_t PITCH_RANGE = 50 * 100;
  static constexpr int32_t PITCH_STEP_FINE = 10;    // 0.1%
  static constexpr int32_t PITCH_STEP_COARSE = 100; // 1%
  static constexpr uint32_t NUDGE_REPEAT_DELAY_MS = 400;
  static constexpr uint32_t NUDGE_REPEAT_INTERVAL_MS = 80;

  enum CutMode : uint8_t { CUT_GATE = 0, CUT_KILL = 1 };

  void Setup(const vector<string>& args) override;
  void Loop() override;
  void End() override;

private:
  CreateSavedVar("SC1000", cutMode, uint8_t, CUT_GATE);
  CreateSavedVar("SC1000", pressureEnabled, bool, true);

  Timer renderTimer;

  // State mirror (the SC1000 never reports back)
  bool cueSet[2][16] = {};
  uint8_t volume[2] = {127, 127}; // Level set by the faders
  bool volumeKnown[2] = {false, false};
  int32_t pitch[2] = {0, 0};
  bool coarsePitch = false;
  bool gateClosed = false; // Gate mode has silenced the samples deck

  // Pad state
  bool held[8][8] = {};
  uint8_t cutPressure[8] = {}; // 7-bit pressure per held cut pad
  uint32_t nudgeHoldStart[8] = {};
  uint32_t nudgeLastRepeat[8] = {};

  // Output
  std::deque<MidiPacket> outQueue;
  uint32_t lastSendUs = 0;
  int16_t sentVolume[2] = {-1, -1};
  uint8_t targetVolume[2] = {127, 127};
  bool volumeDirty[2] = {false, false};
  uint8_t nextVolumeDeck = 0;

  void InputEventHandler(InputEvent& inputEvent);
  void PadEvent(Point xy, KeypadInfo& keypad);
  void CuePad(uint8_t deck, uint8_t cue, KeypadState state);
  void CutPad(uint8_t x, KeypadInfo& keypad);
  void TransportPad(uint8_t x, KeypadState state);
  void PitchPad(uint8_t x, KeypadState state);
  void VolumePad(uint8_t deck, uint8_t x, KeypadState state);

  void Nudge(uint8_t deck, int8_t direction);
  void SetPitch(uint8_t deck, int32_t value);
  void ResetDeckMirror(uint8_t deck);
  void UpdateVolumeTargets();
  void SetCutMode(uint8_t mode);
  void HandleNudgeRepeat();

  void Queue(MidiPacket packet);
  void FlushOutput();
  void SendSpaced(MidiPacket packet);

  void Render();
  static uint8_t FaderLevel(uint8_t x);
};
