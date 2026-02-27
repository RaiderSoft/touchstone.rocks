#include "vision/vision.hpp"

#include <opencv2/imgproc.hpp>

namespace touchstone {

VisionSystem::VisionSystem(int board_size) : board_size_(board_size) {}

VisionSystem::~VisionSystem() { Stop(); }

bool VisionSystem::Start(int camera_index) {
  if (running_) return true;
  camera_index_ = camera_index;
  running_ = true;
  camera_thread_ = std::thread(&VisionSystem::CameraLoop, this);
  return true;
}

void VisionSystem::Stop() {
  running_ = false;
  if (camera_thread_.joinable()) {
    camera_thread_.join();
  }
}

bool VisionSystem::IsRunning() const { return running_; }

DetectionResult VisionSystem::GetLatestDetection() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return latest_detection_;
}

std::vector<unsigned char> VisionSystem::GetLatestFrameRGBA(int& width,
                                                            int& height) const {
  std::lock_guard<std::mutex> lock(mtx_);
  if (latest_frame_.empty()) {
    width = 0;
    height = 0;
    return {};
  }
  width = latest_frame_.cols;
  height = latest_frame_.rows;

  cv::Mat rgba;
  cv::cvtColor(latest_frame_, rgba, cv::COLOR_BGR2RGBA);

  size_t size = rgba.total() * rgba.elemSize();
  std::vector<unsigned char> pixels(size);
  std::memcpy(pixels.data(), rgba.data, size);
  return pixels;
}

std::vector<unsigned char> VisionSystem::GetLatestFrameCannyRGBA(
    int& width, int& height, float threshold) const {
  std::lock_guard<std::mutex> lock(mtx_);
  if (latest_frame_.empty()) {
    width = 0;
    height = 0;
    return {};
  }
  width = latest_frame_.cols;
  height = latest_frame_.rows;

  cv::Mat gray, blurred, edges, rgba;
  cv::cvtColor(latest_frame_, gray, cv::COLOR_BGR2GRAY);
  cv::GaussianBlur(gray, blurred, cv::Size(9, 9), 2);
  cv::Canny(blurred, edges, threshold / 2, threshold);
  cv::cvtColor(edges, rgba, cv::COLOR_GRAY2RGBA);

  size_t size = rgba.total() * rgba.elemSize();
  std::vector<unsigned char> pixels(size);
  std::memcpy(pixels.data(), rgba.data, size);
  return pixels;
}

void VisionSystem::ResetDetection() {
  std::lock_guard<std::mutex> lock(mtx_);
  black_confidence_.clear();
  white_confidence_.clear();
  smoothed_board_.clear();
  latest_detection_ = DetectionResult{};
}

void VisionSystem::SetBoardSize(int size) {
  board_size_ = size;
  black_confidence_.clear();
  white_confidence_.clear();
  smoothed_board_.clear();
  if (calibration_.valid) {
    UpdateTransform();
  }
}

int VisionSystem::GetBoardSize() const { return board_size_; }

bool VisionSystem::IsCalibrated() const { return calibration_.valid; }

void VisionSystem::LoadCalibration() {
  calibration_ = LoadCalibrationData();
  if (calibration_.valid) {
    UpdateTransform();
  }
}

void VisionSystem::SaveCalibration() const {
  SaveCalibrationData(calibration_);
}

CalibrationData& VisionSystem::GetCalibration() { return calibration_; }

void VisionSystem::SetCalibrationCorner(int index, float x, float y) {
  if (index >= 0 && index < 4) {
    calibration_.corners[index][0] = x;
    calibration_.corners[index][1] = y;
  }
}

void VisionSystem::UpdateTransform() {
  // Source: ideal grid corners in normalized [0,1] space.
  //   TL=(0,0)  TR=(1,0)  BL=(0,1)  BR=(1,1)
  // Destination: calibration corners in image pixel coordinates.
  cv::Point2f src[4] = {
      {0.0f, 0.0f},  // TL
      {1.0f, 0.0f},  // TR
      {0.0f, 1.0f},  // BL
      {1.0f, 1.0f},  // BR
  };
  cv::Point2f dst[4] = {
      {calibration_.corners[0][0], calibration_.corners[0][1]},  // TL
      {calibration_.corners[1][0], calibration_.corners[1][1]},  // TR
      {calibration_.corners[2][0], calibration_.corners[2][1]},  // BL
      {calibration_.corners[3][0], calibration_.corners[3][1]},  // BR
  };
  transform_ = cv::getPerspectiveTransform(src, dst);
}

void VisionSystem::GetGridPoint(int row, int col, float& x, float& y) const {
  if (transform_.empty()) {
    x = 0;
    y = 0;
    return;
  }

  // Map from normalized grid coords to image pixel coords via the
  // perspective transform.  This correctly handles camera angle.
  float u = static_cast<float>(col) / (board_size_ - 1);
  float v = static_cast<float>(row) / (board_size_ - 1);

  std::vector<cv::Point2f> in = {{u, v}};
  std::vector<cv::Point2f> out;
  cv::perspectiveTransform(in, out, transform_);

  x = out[0].x;
  y = out[0].y;
}

void VisionSystem::ProcessFrame(const cv::Mat& frame) {
  if (!calibration_.valid || transform_.empty()) return;

  // Convert to grayscale and blur for Hough circle detection.
  cv::Mat gray;
  cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
  cv::GaussianBlur(gray, gray, cv::Size(9, 9), 2);

  // Estimate grid spacing and expected stone radius from center grid points.
  float cx0, cy0, cx1, cy1;
  int mid = board_size_ / 2;
  GetGridPoint(mid, mid, cx0, cy0);
  GetGridPoint(mid, mid + 1, cx1, cy1);
  float dx = cx1 - cx0, dy = cy1 - cy0;
  float grid_spacing = std::sqrt(dx * dx + dy * dy);
  int min_r = std::max(3, static_cast<int>(grid_spacing * 0.25f));
  int max_r = static_cast<int>(grid_spacing * 0.55f);
  float min_dist = grid_spacing * 0.5f;

  // Detect circles (stones) using Hough transform.
  std::vector<cv::Vec3f> circles;
  cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, 1,
                   static_cast<double>(min_dist),
                   static_cast<double>(calibration_.canny_threshold),
                   static_cast<double>(calibration_.detect_sensitivity),
                   min_r, max_r);

  int total = board_size_ * board_size_;

  // Raw per-frame detection.
  std::vector<StoneColor> raw(total, StoneColor::kEmpty);
  std::vector<OffGridCircle> off_grid;

  // Inverse perspective transform: image pixels → normalized grid coords.
  cv::Mat inv_transform;
  cv::invert(transform_, inv_transform);

  // Match each detected circle to its nearest grid intersection.
  float match_dist = grid_spacing * 0.45f;
  for (const auto& c : circles) {
    float ccx = c[0], ccy = c[1], cr = c[2];
    int best_pos = -1;
    float best_d = match_dist;

    for (int row = 0; row < board_size_; row++) {
      for (int col = 0; col < board_size_; col++) {
        float gx, gy;
        GetGridPoint(row, col, gx, gy);
        float d = std::hypot(ccx - gx, ccy - gy);
        if (d < best_d) {
          best_d = d;
          best_pos = row * board_size_ + col;
        }
      }
    }

    if (best_pos < 0) {
      // Circle not near any intersection — record as off-grid.
      if (!inv_transform.empty()) {
        std::vector<cv::Point2f> in = {{ccx, ccy}};
        std::vector<cv::Point2f> out;
        cv::perspectiveTransform(in, out, inv_transform);
        float gr = out[0].y * (board_size_ - 1);
        float gc = out[0].x * (board_size_ - 1);
        if (gr >= -0.5f && gr < board_size_ - 0.5f &&
            gc >= -0.5f && gc < board_size_ - 0.5f) {
          off_grid.push_back({gr, gc});
        }
      }
      continue;
    }

    // Classify by mean brightness within the detected circle.
    int icx = static_cast<int>(ccx), icy = static_cast<int>(ccy);
    int ir = static_cast<int>(cr);
    int x0 = std::max(0, icx - ir);
    int y0 = std::max(0, icy - ir);
    int x1 = std::min(gray.cols, icx + ir);
    int y1 = std::min(gray.rows, icy + ir);
    if (x0 >= x1 || y0 >= y1) continue;

    cv::Mat roi = gray(cv::Rect(x0, y0, x1 - x0, y1 - y0));
    cv::Mat mask = cv::Mat::zeros(roi.size(), CV_8UC1);
    cv::circle(mask, cv::Point(icx - x0, icy - y0), ir, cv::Scalar(255), -1);
    cv::Scalar mean_val = cv::mean(roi, mask);
    float brightness = static_cast<float>(mean_val[0]);

    raw[best_pos] =
        (brightness < calibration_.brightness_midpoint)
            ? StoneColor::kBlack
            : StoneColor::kWhite;
  }

  // Initialize smoothing buffers on first use.
  if ((int)black_confidence_.size() != total) {
    black_confidence_.assign(total, 0);
    white_confidence_.assign(total, 0);
    smoothed_board_.assign(total, StoneColor::kEmpty);
  }

  // Update confidence counters with hysteresis.
  for (int i = 0; i < total; i++) {
    // Black confidence.
    if (raw[i] == StoneColor::kBlack) {
      if (black_confidence_[i] < kSmoothMax) black_confidence_[i]++;
    } else {
      if (black_confidence_[i] > 0) black_confidence_[i]--;
    }

    // White confidence.
    if (raw[i] == StoneColor::kWhite) {
      if (white_confidence_[i] < kSmoothMax) white_confidence_[i]++;
    } else {
      if (white_confidence_[i] > 0) white_confidence_[i]--;
    }

    // Apply hysteresis to determine smoothed state.
    StoneColor prev = smoothed_board_[i];
    if (prev == StoneColor::kBlack) {
      // Stone disappears only when confidence drops below threshold.
      if (black_confidence_[i] < kDisappearThreshold)
        smoothed_board_[i] = StoneColor::kEmpty;
    } else if (prev == StoneColor::kWhite) {
      if (white_confidence_[i] < kDisappearThreshold)
        smoothed_board_[i] = StoneColor::kEmpty;
    } else {
      // Empty — stone appears only when confidence exceeds threshold.
      if (black_confidence_[i] >= kAppearThreshold)
        smoothed_board_[i] = StoneColor::kBlack;
      else if (white_confidence_[i] >= kAppearThreshold)
        smoothed_board_[i] = StoneColor::kWhite;
    }
  }

  DetectionResult result;
  result.board = smoothed_board_;
  result.off_grid = off_grid;
  result.board_found = true;
  result.confidence = 1.0f;
  result.frame_number = frame_counter_;

  std::lock_guard<std::mutex> lock(mtx_);
  latest_detection_ = result;
}

void VisionSystem::CameraLoop() {
  cv::VideoCapture cap(camera_index_, cv::CAP_V4L2);
  if (!cap.isOpened()) {
    cap.open(camera_index_);
    if (!cap.isOpened()) {
      running_ = false;
      return;
    }
  }

  while (running_) {
    cv::Mat frame;
    if (!cap.read(frame) || frame.empty()) continue;

    // Camera faces the user from across the table — rotate 180°.
    cv::flip(frame, frame, -1);

    {
      std::lock_guard<std::mutex> lock(mtx_);
      latest_frame_ = frame.clone();
      frame_counter_++;
    }

    ProcessFrame(frame);
  }

  cap.release();
}

}  // namespace touchstone
