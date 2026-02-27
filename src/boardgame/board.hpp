#pragma once

#include <string>

constexpr int kEmpty = 0;
constexpr int kPlayer1 = 1;   // Black
constexpr int kPlayer2 = 2;   // White

void InitMorrisBoard();
void InitGoBoard(int size);
void SetupGoBoard(int size);
void CloseBoard();
bool ShouldClose();
void BeginFrame();
void EndFrame();
void DrawPiece(int position, int player);
void DrawHighlight(int position);
void DrawTerritory(int position, int player);
void DrawStatus(const std::string& text);
int GetClickedPosition();
bool IsPassPressed();
