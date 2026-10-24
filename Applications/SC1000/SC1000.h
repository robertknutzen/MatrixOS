// SC1000: one-way USB MIDI controller for the Portablism SC1000/SC500.
// Spec: "SC1000 Controller app for Mystrix Pro — build plan.md" in the repo root.
#pragma once

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

  void Setup(const vector<string>& args) override;
  void Loop() override;
  void End() override;

private:
  Timer renderTimer;
  bool testPadHeld = false;

  void InputEventHandler(InputEvent& inputEvent);
  void Render();
};
