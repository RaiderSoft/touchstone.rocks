#pragma once

#include <vector>

#include "cards.hpp"
#include "chat/chat_overlay.hpp"
#include "engine/katago_engine.hpp"
#include "vision/vision.hpp"

void RunPuzzleMode(const std::vector<touchstone::Card>& deck,
                   ChatOverlay& chat, katago::Engine* katago,
                   touchstone::VisionSystem* vision,
                   bool vision_active, int vision_board_size);
