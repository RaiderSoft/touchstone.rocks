#pragma once

enum class MenuOption {
    Morris,
    Go9,
    Go13,
    Go19,
    Quit,
    Count
};

void InitMenu();
void CloseMenu();
void DrawMenu(int selected);
int GetMenuInput(int current);
bool IsMenuConfirmed();
bool ShouldCloseMenu();
