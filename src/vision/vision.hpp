#pragma once

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include "vision/calibration.hpp"

namespace touchstone {

enum class StoneColor { kEmpty = 0, kBlack = 1, kWhite = 2 };

// A circle detected by vision that didn't snap to any grid intersection.
struct OffGridCircle {
  float row;  // Fractional row (0-based, in grid units).
  float col;  // Fractional col (0-based, in grid units).
};

struct DetectionResult {
  std::vector<StoneColor> board;  // size*size flat array
  std::vector<OffGridCircle> off_grid;  // Circles not on any intersection.
  bool board_found = false;
  float confidence = 0.0f;
  int frame_number = 0;
};

class VisionSystem {
 public:
  explicit VisionSystem(int board_size);
  ~VisionSystem();

  // Start/stop camera thread.
  bool Start(int camera_index = 0);
  void Stop();
  bool IsRunning() const;

  // Thread-safe: get latest detection result.
  DetectionResult GetLatestDetection() const;

  // Thread-safe: get latest camera frame as RGBA pixels for Raylib.
  // Returns empty vector if no frame available.
  std::vector<unsigned char> GetLatestFrameRGBA(int& width, int& height) const;

  // Thread-safe: get Canny edge view of latest frame as RGBA pixels.
  std::vector<unsigned char> GetLatestFrameCannyRGBA(int& width, int& height,
                                                     float threshold) const;

  // Calibration.
  bool IsCalibrated() const;
  void LoadCalibration();
  void SaveCalibration() const;
  CalibrationData& GetCalibration();

  // Clear detection state (smoothing counters + latest result).
  void ResetDetection();

  // Change the board size and reset detection state.
  void SetBoardSize(int size);
  int GetBoardSize() const;

  // Set a corner during interactive calibration (0=TL, 1=TR, 2=BL, 3=BR).
  void SetCalibrationCorner(int index, float x, float y);

  // Recompute the perspective transform from the current calibration corners.
  void UpdateTransform();

  // Compute grid intersection in image coordinates using perspective transform.
  void GetGridPoint(int row, int col, float& x, float& y) const;

 private:
  void CameraLoop();
  void ProcessFrame(const cv::Mat& frame);

  int board_size_;
  std::atomic<bool> running_{false};
  std::thread camera_thread_;
  int camera_index_ = 0;

  mutable std::mutex mtx_;
  // Protected by mtx_:
  DetectionResult latest_detection_;
  cv::Mat latest_frame_;
  int frame_counter_ = 0;

  CalibrationData calibration_;

  // Perspective transform: maps ideal grid coords to image pixel coords.
  // Computed from the 4 calibration corners.
  cv::Mat transform_;  // 3x3 perspective matrix

  // Temporal smoothing: per-position confidence counters for hysteresis.
  // Positive = frames confirming a stone, negative ticks toward empty.
  std::vector<int> black_confidence_;   // per-position, 0..SMOOTH_MAX
  std::vector<int> white_confidence_;
  std::vector<StoneColor> smoothed_board_;
  static constexpr int kSmoothMax = 10;
  static constexpr int kAppearThreshold = 6;   // frames to confirm appearance
  static constexpr int kDisappearThreshold = 3; // frames below this → gone
};

}  // namespace touchstone
