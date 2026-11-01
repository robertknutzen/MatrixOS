// Breakbeat: grid controller for the rpi-sampler breakbeat track (Raspberry Pi sampler).
// The MIDI contract is docs/midi-spec.md in the rpi-sampler repo; the numbers below must match it.
//
// Play page: the 8x8 grid is the loop's 64 slices. Pads light from the sampler's feedback.
// Settings page (tap the function key): every setting and action as pads.
#pragma once

#include "MatrixOS.h"
#include "Application.h"

class Breakbeat : public Application {
public:
  inline static Application_Info info = {
      .name = "Breakbeat",
      .author = "b0b",
      .color = Color(0xFF9000),
      .version = 1,
      .visibility = true,
  };

  // ---- MIDI contract (docs/midi-spec.md) ----
  static constexpr uint8_t CHANNEL = 0;     // 0-based: MIDI channel 1
  static constexpr uint8_t FIRST_NOTE = 36; // slice 0; slice = note - 36
  static constexpr uint8_t CC_SLICE_RES = 20;
  static constexpr uint8_t CC_BARS = 21;
  static constexpr uint8_t CC_DIRECTION = 22;
  static constexpr uint8_t CC_SLICE_FX = 23;
  static constexpr uint8_t CC_FOLLOW = 24;
  static constexpr uint8_t CC_QUANTIZE = 25;
  static constexpr uint8_t CC_REC_MODE = 26;
  static constexpr uint8_t CC_REC_BARS = 27;
  static constexpr uint8_t CC_FILTER = 74;
  static constexpr uint8_t CC_CRUSH = 75;
  static constexpr uint8_t CC_REC_ARM = 80;
  static constexpr uint8_t CC_REC_STOP = 81;
  static constexpr uint8_t CC_UNDO = 82;
  static constexpr uint8_t CC_LOAD_NEXT = 83;
  static constexpr uint8_t CC_LOAD_PREV = 84;
  static constexpr uint8_t CC_PANIC = 85;
  static constexpr uint8_t CC_REFRESH = 86;
  static constexpr uint8_t CC_ALIVE = 87;   // keep-alive: the sampler only sends feedback while this arrives
  static constexpr uint8_t CC_TRANSPORT = 88; // 127 play from the top, 0 stop; echoed back
  static constexpr uint8_t CC_REC_STATE = 90; // feedback only

  // Pad LED states sent by the sampler
  enum Led : uint8_t { LED_OFF = 0, LED_SLICE = 1, LED_PLAYHEAD = 2, LED_HELD = 3, LED_QUEUED = 4 };
  enum RecState : uint8_t { REC_IDLE = 0, REC_ARMED = 1, REC_RECORDING = 2, REC_BUSY = 3 };

  // The launcher opens apps on release; ignore the bounce of that pad as the finger lifts
  static constexpr uint32_t LAUNCH_IGNORE_MS = 250;
  // Ask for a full refresh this often until the sampler answers
  static constexpr uint32_t REFRESH_RETRY_MS = 2000;
  // Keep-alive interval (the sampler stops feedback 3 s after the last one)
  static constexpr uint32_t ALIVE_INTERVAL_MS = 1000;
  // No feedback for this long: show the offline look and start asking again
  static constexpr uint32_t FEEDBACK_TIMEOUT_MS = 5000;

  enum Page : uint8_t { PAGE_PLAY, PAGE_SETTINGS };

  void Setup(const vector<string>& args) override;
  void Loop() override;
  void End() override;

private:
  Page page = PAGE_PLAY;
  uint32_t launchMs = 0;
  uint32_t lastRefreshMs = 0;
  uint32_t lastAliveMs = 0;
  uint32_t lastFeedbackMs = 0;
  Timer renderTimer;
  bool held[8][8] = {};

  // Mirror of the sampler, filled from its feedback
  bool connected = false; // any feedback seen yet
  uint8_t led[64] = {};
  uint8_t cc[128] = {};   // last value per CC, from the settings echo

  // MIDI
  void Send(MidiPacket packet);
  void SendCC(uint8_t controller, uint8_t value);
  void HandleMidi(const MidiPacket& packet);
  void RequestRefresh();
  void ReleaseAllSlices();

  // Option helpers: settings with n options split 0-127 into equal ranges
  static uint8_t OptionIndex(uint8_t value, uint8_t n);
  static uint8_t OptionValue(uint8_t index, uint8_t n);
  uint8_t Option(uint8_t controller, uint8_t n) { return OptionIndex(cc[controller], n); }

  // Pages
  void InputEventHandler(InputEvent& inputEvent);
  void PlayEvent(Point xy, KeypadState state);
  void SettingsEvent(Point xy, KeypadState state);
  void Render();
  void RenderPlay();
  void RenderSettings();
  Color Lit(Color color, bool on, Point xy);
  uint8_t SlicesPerBeat();
};
