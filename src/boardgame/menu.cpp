#include "menu.hpp"

#include "raylib.h"

// Menu window dimensions.
static constexpr int kMenuWidth = 600;
static constexpr int kMenuHeight = 400;

// Colors for the menu UI.
static const Color kBgColor = {30, 30, 40, 255};
static const Color kTitleColor = {220, 200, 160, 255};
static const Color kTextColor = {180, 180, 180, 255};
static const Color kSelectedColor = {255, 255, 255, 255};
static const Color kHighlightBg = {60, 60, 80, 255};
static const Color kHintColor = {120, 120, 130, 255};

// Menu item labels.
static const char* kMenuLabels[] = {
    "Nine Men's Morris",
    "Go (9x9)",
    "Go (13x13)",
    "Go (19x19)",
    "Quit"
};
static constexpr int kNumItems = static_cast<int>(MenuOption::Count);

// Layout constants.
static constexpr int kTitleY = 60;
static constexpr int kTitleFontSize = 40;
static constexpr int kItemFontSize = 24;
static constexpr int kItemHeight = 40;
static constexpr int kMenuStartY = 140;
static constexpr int kHintY = 360;
static constexpr int kHintFontSize = 16;

// Track if mouse is hovering over an item (for click detection).
static int hoveredItem = -1;
static bool clickConfirmed = false;

void InitMenu() {
    InitWindow(kMenuWidth, kMenuHeight, "Board Games");
    SetTargetFPS(60);
    SetExitKey(0);
    hoveredItem = -1;
    clickConfirmed = false;
}

void CloseMenu() {
    CloseWindow();
}

void DrawMenu(int selected) {
    BeginDrawing();
    ClearBackground(kBgColor);

    // Draw title centered.
    const char* title = "BOARD GAMES";
    int titleWidth = MeasureText(title, kTitleFontSize);
    DrawText(title, (kMenuWidth - titleWidth) / 2, kTitleY, kTitleFontSize, kTitleColor);

    // Draw menu items.
    for (int i = 0; i < kNumItems; i++) {
        int y = kMenuStartY + i * kItemHeight;
        int textWidth = MeasureText(kMenuLabels[i], kItemFontSize);
        int x = (kMenuWidth - textWidth) / 2;

        // Draw highlight background for selected item.
        if (i == selected) {
            int bgPadding = 10;
            int bgWidth = textWidth + bgPadding * 2 + 30;  // Extra for arrow
            int bgX = (kMenuWidth - bgWidth) / 2;
            DrawRectangle(bgX, y - 5, bgWidth, kItemHeight - 5, kHighlightBg);

            // Draw selection arrow.
            int arrowX = bgX + 10;
            DrawText(">", arrowX, y, kItemFontSize, kSelectedColor);

            DrawText(kMenuLabels[i], x, y, kItemFontSize, kSelectedColor);
        } else {
            DrawText(kMenuLabels[i], x, y, kItemFontSize, kTextColor);
        }
    }

    // Draw control hints.
    const char* hints = "Up/Down: Select    Enter: Confirm";
    int hintsWidth = MeasureText(hints, kHintFontSize);
    DrawText(hints, (kMenuWidth - hintsWidth) / 2, kHintY, kHintFontSize, kHintColor);

    EndDrawing();
}

int GetMenuInput(int current) {
    // Keyboard navigation.
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        current--;
        if (current < 0) current = kNumItems - 1;
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        current++;
        if (current >= kNumItems) current = 0;
    }

    // Mouse hover detection.
    Vector2 mouse = GetMousePosition();
    hoveredItem = -1;
    for (int i = 0; i < kNumItems; i++) {
        int y = kMenuStartY + i * kItemHeight;
        int textWidth = MeasureText(kMenuLabels[i], kItemFontSize);
        int bgWidth = textWidth + 50;
        int bgX = (kMenuWidth - bgWidth) / 2;

        if (mouse.x >= bgX && mouse.x <= bgX + bgWidth &&
            mouse.y >= y - 5 && mouse.y <= y + kItemHeight - 10) {
            hoveredItem = i;
            current = i;
            break;
        }
    }

    // Track click on hovered item.
    clickConfirmed = false;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hoveredItem >= 0) {
        clickConfirmed = true;
    }

    return current;
}

bool IsMenuConfirmed() {
    return IsKeyPressed(KEY_ENTER) || clickConfirmed;
}

bool ShouldCloseMenu() {
    return WindowShouldClose();
}
