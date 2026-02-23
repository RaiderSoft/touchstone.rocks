#pragma once

#include "cards.h"
#include "vision.h"

// Interactive camera calibration and detection debug mode.
void RunVisionDevMode();

// Detect a new stone placed on the board using vision, with temporal
// confirmation. Returns the position (0-80) if a stable new stone is
// detected, or -1 if not yet confirmed.
int DetectVisionMove(touchstone::VisionSystem& vision,
                     const touchstone::Card& card,
                     touchstone::DetectionResult& baseline,
                     bool& baseline_captured);
