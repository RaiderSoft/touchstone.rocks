#pragma once

#include <vector>

#include "engine/katago_engine.hpp"
#include "ui/game_board.hpp"
#include "ui/game_mode.hpp"

void DrawOwnershipOverlay(const GameBoard& gb,
                          const katago::AnalysisResult& analysis);
void DrawTopMoves(const GameBoard& gb,
                  const katago::AnalysisResult& analysis);
Color QualityColor(float quality);
void DrawKataGoStatsPanel(const GameBoard& gb,
                          const katago::AnalysisResult& analysis,
                          go::Stone current_player,
                          const std::vector<MoveRecord>& move_log,
                          go::Stone human_color);
void DrawMoveQuality(const GameBoard& gb, int pos, float quality,
                     bool pending, bool show_winrate);
