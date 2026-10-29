// PadButton: a one-pad button that acts the moment it's pressed and lights fully while held.
// (The OS UIButton acts on release, which feels slow for performance controls.)
#pragma once

#include "MatrixOS.h"
#include "UI/UI.h"

class PadButton : public UIComponent {
public:
  template <typename F> void OnPress(F&& f) { pressCallback = UICallback<void()>(static_cast<F&&>(f)); }

  // Brightness when not held, so a button can show state (e.g. how far the octave is shifted)
  template <typename F> void SetBrightnessFunc(F&& f) { brightnessFunc = UICallback<uint8_t()>(static_cast<F&&>(f)); }
  void SetColor(Color newColor) { color = newColor; }

  Dimension GetSize() override { return Dimension(1, 1); }

  bool KeyEvent(Point xy, KeypadInfo* keypadInfo) override {
    if (keypadInfo->state == KeypadState::Pressed)
    {
      down = true;
      if (pressCallback)
        pressCallback();
    }
    else if (keypadInfo->state == KeypadState::Released)
    {
      down = false;
    }
    return true;
  }

  bool Render(Point origin) override {
    uint8_t level = down ? 255 : (brightnessFunc ? brightnessFunc() : 255);
    MatrixOS::LED::SetColor(origin, color.Dim(level));
    return true;
  }

private:
  Color color = Color::White;
  bool down = false;
  UICallback<void()> pressCallback;
  UICallback<uint8_t()> brightnessFunc;
};
