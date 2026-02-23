#include "vision/calibration.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace touchstone {

std::string CalibrationPath() {
  const char* home = std::getenv("HOME");
  if (!home) home = ".";
  std::string dir = std::string(home) + "/.touchstone";
  mkdir(dir.c_str(), 0755);
  return dir + "/calibration.txt";
}

CalibrationData LoadCalibrationData() {
  CalibrationData data;
  std::ifstream in(CalibrationPath());
  if (!in.is_open()) return data;

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream iss(line);
    std::string key;
    iss >> key;
    if (key == "corner") {
      int idx;
      iss >> idx >> data.corners[idx][0] >> data.corners[idx][1];
    } else if (key == "detect_sensitivity") {
      iss >> data.detect_sensitivity;
    } else if (key == "brightness_midpoint") {
      iss >> data.brightness_midpoint;
    } else if (key == "canny_threshold") {
      iss >> data.canny_threshold;
    } else if (key == "valid") {
      int v;
      iss >> v;
      data.valid = (v != 0);
    }
  }
  return data;
}

void SaveCalibrationData(const CalibrationData& data) {
  std::ofstream out(CalibrationPath());
  out << "# Touchstone Board Calibration\n";
  for (int i = 0; i < 4; i++) {
    out << "corner " << i << " " << data.corners[i][0] << " "
        << data.corners[i][1] << "\n";
  }
  out << "detect_sensitivity " << data.detect_sensitivity << "\n";
  out << "brightness_midpoint " << data.brightness_midpoint << "\n";
  out << "canny_threshold " << data.canny_threshold << "\n";
  out << "valid " << (data.valid ? 1 : 0) << "\n";
}

}  // namespace touchstone
