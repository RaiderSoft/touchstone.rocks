#include "progress.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <sys/stat.h>

namespace touchstone {

// SM-2 inspired interval schedule based on streak.
static double BaseInterval(int streak, double ease) {
  switch (streak) {
    case 0: return 60;            // 1 minute (in-session retry)
    case 1: return 600;           // 10 minutes
    case 2: return 86400;         // 1 day
    case 3: return 3 * 86400;    // 3 days
    default:
      return 3 * 86400 * std::pow(ease, streak - 3);
  }
}

ProgressTracker::ProgressTracker(const std::vector<Card>& deck,
                                 const std::vector<SkillNode>& tree)
    : deck_(deck), tree_(tree) {
  for (int i = 0; i < static_cast<int>(deck_.size()); i++) {
    card_index_[deck_[i].id] = i;
  }
}

std::string ProgressTracker::ProgressPath() const {
  const char* home = std::getenv("HOME");
  if (!home) home = ".";
  std::string dir = std::string(home) + "/.touchstone";
  mkdir(dir.c_str(), 0755);
  return dir + "/progress.txt";
}

void ProgressTracker::Load() {
  data_.clear();
  std::ifstream in(ProgressPath());
  if (!in.is_open()) return;

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream iss(line);
    std::string id;
    CardProgress p;
    long long nr;
    if (iss >> id >> p.times_correct >> p.times_seen >> p.streak >> p.ease >>
        nr) {
      p.next_review = static_cast<time_t>(nr);
      data_[id] = p;
    }
  }
}

void ProgressTracker::Save() const {
  std::ofstream out(ProgressPath());
  out << "# Touchstone Progress\n";
  out << "# card_id correct seen streak ease next_review\n";
  for (const auto& [id, p] : data_) {
    out << id << " " << p.times_correct << " " << p.times_seen << " "
        << p.streak << " " << p.ease << " "
        << static_cast<long long>(p.next_review) << "\n";
  }
}

CardProgress ProgressTracker::GetProgress(const std::string& card_id) const {
  auto it = data_.find(card_id);
  if (it != data_.end()) return it->second;
  return {};
}

// Category weight: weaker nodes get shorter intervals (more frequent review).
// Returns a multiplier > 1.0 for weak nodes, < 1.0 for strong nodes.
double ProgressTracker::CategoryWeight(const std::string& node_id) const {
  double score = NodeScore(node_id);
  // score 0.0 -> weight 2.0 (half intervals, twice as frequent)
  // score 1.0 -> weight 1.0 (normal intervals)
  return 2.0 - score;
}

void ProgressTracker::RecordReview(int deck_index, bool correct) {
  const Card& card = deck_[deck_index];
  CardProgress& p = data_[card.id];

  p.times_seen++;
  if (correct) {
    p.times_correct++;
    p.streak++;
    if (p.streak > 2) p.ease = std::min(3.0, p.ease + 0.05);
  } else {
    p.streak = 0;
    p.ease = std::max(1.3, p.ease - 0.2);
  }

  // Find which node this card belongs to.
  std::string node_id;
  for (const auto& node : tree_) {
    for (const auto& cid : node.card_ids) {
      if (cid == card.id) {
        node_id = node.id;
        break;
      }
    }
    if (!node_id.empty()) break;
  }

  double interval = BaseInterval(p.streak, p.ease);
  if (!node_id.empty()) {
    interval /= CategoryWeight(node_id);
  }
  p.next_review = std::time(nullptr) + static_cast<time_t>(interval);
}

// Node status: mastered if score >= threshold and all cards seen at least once.
// Unlocked if all prerequisites are mastered. Locked otherwise.
NodeStatus ProgressTracker::GetNodeStatus(const std::string& node_id) const {
  const SkillNode* node = nullptr;
  for (const auto& n : tree_) {
    if (n.id == node_id) {
      node = &n;
      break;
    }
  }
  if (!node) return NodeStatus::kLocked;

  // Check prerequisites.
  for (const auto& req : node->requires) {
    if (GetNodeStatus(req) != NodeStatus::kMastered) {
      return NodeStatus::kLocked;
    }
  }

  // Check mastery: all cards seen AND score >= threshold.
  if (NodeCardsSeen(node_id) == static_cast<int>(node->card_ids.size()) &&
      NodeScore(node_id) >= node->mastery_threshold) {
    return NodeStatus::kMastered;
  }

  return NodeStatus::kUnlocked;
}

double ProgressTracker::NodeScore(const std::string& node_id) const {
  int correct = 0, total = 0;
  for (const auto& node : tree_) {
    if (node.id != node_id) continue;
    for (const auto& cid : node.card_ids) {
      auto it = data_.find(cid);
      if (it != data_.end()) {
        correct += it->second.times_correct;
        total += it->second.times_seen;
      }
    }
  }
  return total > 0 ? static_cast<double>(correct) / total : 0.0;
}

int ProgressTracker::NodeCardsSeen(const std::string& node_id) const {
  int seen = 0;
  for (const auto& node : tree_) {
    if (node.id != node_id) continue;
    for (const auto& cid : node.card_ids) {
      auto it = data_.find(cid);
      if (it != data_.end() && it->second.times_seen > 0) {
        seen++;
      }
    }
  }
  return seen;
}

std::vector<int> ProgressTracker::BuildQueueFrom(
    const std::vector<int>& eligible, int max_new) const {
  time_t now = std::time(nullptr);
  std::vector<int> due;
  std::vector<int> new_cards;
  std::vector<int> upcoming;

  for (int idx : eligible) {
    auto it = data_.find(deck_[idx].id);
    if (it == data_.end() || it->second.times_seen == 0) {
      new_cards.push_back(idx);
    } else if (it->second.next_review <= now) {
      due.push_back(idx);
    } else {
      upcoming.push_back(idx);
    }
  }

  // Sort due by most overdue first.
  std::sort(due.begin(), due.end(), [&](int a, int b) {
    auto pa = data_.find(deck_[a].id);
    auto pb = data_.find(deck_[b].id);
    return pa->second.next_review < pb->second.next_review;
  });

  // Sort upcoming by soonest.
  std::sort(upcoming.begin(), upcoming.end(), [&](int a, int b) {
    auto pa = data_.find(deck_[a].id);
    auto pb = data_.find(deck_[b].id);
    return pa->second.next_review < pb->second.next_review;
  });

  std::vector<int> queue = due;

  // Add new cards. If no due cards, add all new cards (first-time experience).
  int num_new =
      due.empty()
          ? static_cast<int>(new_cards.size())
          : std::min(max_new, static_cast<int>(new_cards.size()));
  for (int i = 0; i < num_new; i++) {
    queue.push_back(new_cards[i]);
  }

  // If queue is still empty, pull the soonest upcoming.
  if (queue.empty()) {
    int n = std::min(5, static_cast<int>(upcoming.size()));
    for (int i = 0; i < n; i++) {
      queue.push_back(upcoming[i]);
    }
  }

  return queue;
}

std::vector<int> ProgressTracker::BuildReviewQueue(int max_new) const {
  // Collect all card indices from unlocked or mastered nodes.
  std::set<std::string> eligible_ids;
  for (const auto& node : tree_) {
    NodeStatus status = GetNodeStatus(node.id);
    if (status == NodeStatus::kLocked) continue;
    for (const auto& cid : node.card_ids) {
      eligible_ids.insert(cid);
    }
  }

  std::vector<int> eligible;
  for (const auto& cid : eligible_ids) {
    auto it = card_index_.find(cid);
    if (it != card_index_.end()) {
      eligible.push_back(it->second);
    }
  }

  return BuildQueueFrom(eligible, max_new);
}

std::vector<int> ProgressTracker::BuildNodeQueue(const std::string& node_id,
                                                 int max_new) const {
  std::vector<int> eligible;
  for (const auto& node : tree_) {
    if (node.id != node_id) continue;
    for (const auto& cid : node.card_ids) {
      auto it = card_index_.find(cid);
      if (it != card_index_.end()) {
        eligible.push_back(it->second);
      }
    }
  }
  return BuildQueueFrom(eligible, max_new);
}

std::string ProgressTracker::FormatTree() const {
  std::ostringstream out;
  out << "=== Touchstone Skill Tree ===\n\n";

  for (const auto& node : tree_) {
    NodeStatus status = GetNodeStatus(node.id);
    int seen = NodeCardsSeen(node.id);
    int total = static_cast<int>(node.card_ids.size());
    int pct = static_cast<int>(NodeScore(node.id) * 100);

    switch (status) {
      case NodeStatus::kMastered:
        out << "  [*] " << node.name << " (" << seen << "/" << total
            << ", " << pct << "%) MASTERED\n";
        break;
      case NodeStatus::kUnlocked:
        out << "  [ ] " << node.name << " (" << seen << "/" << total
            << ", " << pct << "%)\n";
        out << "      " << node.description << "\n";
        break;
      case NodeStatus::kLocked:
        out << "  [#] " << node.name << " (requires:";
        for (const auto& r : node.requires) out << " " << r;
        out << ")\n";
        break;
    }
  }

  out << "\nUsage: touchstone [node-id]   Review a specific node\n";
  out << "       touchstone            Review all unlocked cards\n";
  out << "       touchstone tree       Show this skill tree\n";
  out << "       touchstone reset      Clear all progress\n";
  return out.str();
}

std::string ProgressTracker::SessionSummary(int session_correct,
                                            int session_total) const {
  std::ostringstream out;
  out << "Session: " << session_correct << "/" << session_total;

  for (const auto& node : tree_) {
    NodeStatus status = GetNodeStatus(node.id);
    if (status == NodeStatus::kLocked) continue;
    int pct = static_cast<int>(NodeScore(node.id) * 100);
    out << " | " << node.name << ": " << pct << "%";
    if (status == NodeStatus::kMastered) out << "*";
  }

  return out.str();
}

}  // namespace touchstone
