#pragma once

#include <string>

namespace touchstone {

struct CalibrationData {
  // Four corners in image pixel coordinates: TL, TR, BL, BR.
  float corners[4][2] = {};

  // HoughCircles accumulator threshold — lower = more sensitive.
  float detect_sensitivity = 30.0f;

  // Brightness below this = black stone, above = white stone.
  float brightness_midpoint = 128.0f;

  // Canny upper threshold (used by HoughCircles internally and for debug view).
  float canny_threshold = 100.0f;

  bool valid = false;
};

std::string CalibrationPath();
CalibrationData LoadCalibrationData();
void SaveCalibrationData(const CalibrationData& data);

}  // namespace touchstone
