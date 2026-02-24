#pragma once

#include <string>
#include <vector>

#include "engine/katago_engine.hpp"
#include "go/board.hpp"
#include "go/game.hpp"
#include "ui/game_mode.hpp"

std::string BuildGameContext(
    const go::Game& game, int gp_val, go::Stone human_color,
    const go::Board& setup_board, bool setup_mode,
    const katago::AnalysisResult& last_analysis, bool vision_active,
    int board_size, const std::vector<MoveRecord>& move_log);
