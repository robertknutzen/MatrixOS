// ChordGrid: a block of triad pads. Columns walk up the scale; each row has its own inversion and octave.
// Any screen can place one, with its own size and row setup.
#pragma once

#include <vector>
#include "ChordMachine.h"
#include "UI/UI.h"

class ChordGrid : public UIComponent {
public:
  struct Row {
    uint8_t inversion; // 0 = root position, 1 = Inv1, 2 = Inv2
    int8_t octaveShift; // Octaves above the app's current octave
  };

  // rows: top to bottom. The grid is 8 pads wide and one pad tall per row.
  ChordGrid(ChordMachine* app, std::vector<Row> rows) : app(app), rows(rows) {}

  Dimension GetSize() override { return Dimension(WIDTH, rows.size()); }

  bool KeyEvent(Point xy, KeypadInfo* keypadInfo) override {
    Held& pad = held[xy.x][xy.y];

    if (keypadInfo->state == KeypadState::Pressed)
    {
      ChordMachine::Chord chord = ChordAt(xy);
      if (!chord.valid)
        return true;
      uint8_t velocity = keypadInfo->velocity.to7bits();
      app->ChordOn(chord, velocity == 0 ? 127 : velocity);
      pad.down = true;
      pad.chord = chord; // Keep what started, so an octave change mid-hold still stops the right note
    }
    else if (keypadInfo->state == KeypadState::Released && pad.down)
    {
      app->ChordOff(pad.chord);
      pad.down = false;
    }
    return true; // Also swallows Hold, so holding a pad doesn't scroll the screen name
  }

  bool Render(Point origin) override {
    for (uint8_t y = 0; y < rows.size(); y++)
    {
      for (uint8_t x = 0; x < WIDTH; x++)
      {
        ChordMachine::Chord chord = ChordAt(Point(x, y));
        if (!chord.valid)
          continue; // UI clears the grid before each frame
        Color color = chord.color;
        if (!held[x][y].down)
          color = color.Dim(chord.degree == 0 ? TONIC : IDLE);
        MatrixOS::LED::SetColor(origin + Point(x, y), color);
      }
    }
    return true;
  }

  // Stop anything still held (call before leaving the screen)
  void ReleaseAll() {
    for (auto& column : held)
      for (Held& pad : column)
        if (pad.down)
        {
          app->ChordOff(pad.chord);
          pad.down = false;
        }
  }

  ~ChordGrid() { ReleaseAll(); }

private:
  static constexpr uint8_t WIDTH = 8;
  static constexpr uint8_t MAX_ROWS = 8;
  static constexpr uint8_t IDLE = 60;   // Other degrees
  static constexpr uint8_t TONIC = 160; // Key root column; held pads go to full brightness

  struct Held {
    bool down = false;
    ChordMachine::Chord chord;
  };

  ChordMachine* app;
  std::vector<Row> rows;
  Held held[WIDTH][MAX_ROWS];

  ChordMachine::Chord ChordAt(Point xy) {
    const Row& row = rows[xy.y];
    return app->GetChord(xy.x, row.inversion, row.octaveShift);
  }
};
