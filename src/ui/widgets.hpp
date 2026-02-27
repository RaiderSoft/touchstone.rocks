#pragma once

#include "raylib.h"

struct ButtonStyle {
  Color bg;
  Color bg_hover;
  Color accent;
  Color text;
};

class Button {
 public:
  Button(const char* label, ButtonStyle style, int font_size = 24)
      : label_(label), style_(style), font_size_(font_size) {}

  // Draw the button and return true if clicked.
  bool Draw(float x, float y, float w, float h, Vector2 mouse) const {
    Rectangle rect = {x, y, w, h};
    bool hover = CheckCollisionPointRec(mouse, rect);

    DrawRectangleRec(rect, hover ? style_.bg_hover : style_.bg);
    DrawRectangle(x, y, 4, h, style_.accent);

    int text_y = y + (h - font_size_) / 2;
    DrawText(label_, x + 18, text_y, font_size_, style_.text);

    if (hover) DrawRectangleLinesEx(rect, 1, style_.accent);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  }

 private:
  const char* label_;
  ButtonStyle style_;
  int font_size_;
};

// Predefined button styles.
namespace ui {

inline ButtonStyle GreenStyle() {
  return {{40, 55, 40, 255},
          {55, 70, 55, 255},
          {100, 200, 100, 255},
          {180, 255, 180, 255}};
}

inline ButtonStyle BlueStyle() {
  return {{40, 40, 55, 255},
          {55, 55, 70, 255},
          {100, 140, 220, 255},
          {160, 190, 255, 255}};
}

inline ButtonStyle PurpleStyle() {
  return {{45, 40, 55, 255},
          {60, 55, 70, 255},
          {180, 140, 220, 255},
          {200, 180, 255, 255}};
}

inline ButtonStyle GoldStyle() {
  return {{50, 45, 38, 255},
          {70, 65, 55, 255},
          {220, 180, 100, 255},
          {255, 220, 140, 255}};
}

inline ButtonStyle RedStyle() {
  return {{50, 38, 38, 255},
          {70, 45, 45, 255},
          {180, 80, 80, 255},
          {255, 140, 140, 255}};
}

inline ButtonStyle SubtleStyle() {
  return {{45, 40, 40, 255},
          {60, 55, 55, 255},
          {120, 120, 120, 255},
          {160, 160, 160, 255}};
}

}  // namespace ui
