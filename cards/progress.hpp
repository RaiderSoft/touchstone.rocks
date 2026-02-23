#pragma once

#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

#include "cards.hpp"
#include "nodes.hpp"

namespace touchstone {

struct CardProgress {
  int times_correct = 0;
  int times_seen = 0;
  int streak = 0;
  double ease = 2.5;
  time_t next_review = 0;
};

enum class NodeStatus { kLocked, kUnlocked, kMastered };

class ProgressTracker {
 public:
  // Initialize with the full card deck and skill tree.
  ProgressTracker(const std::vector<Card>& deck,
                  const std::vector<SkillNode>& tree);

  void Load();
  void Save() const;

  // Record a review result and update spaced repetition scheduling.
  void RecordReview(int deck_index, bool correct);

  // Build a review queue of card indices into the deck.
  // Only draws from unlocked/mastered nodes.
  // Due cards first, then new cards (up to max_new).
  // If nothing is due, returns the soonest-upcoming cards.
  std::vector<int> BuildReviewQueue(int max_new = 5) const;

  // Build a review queue filtered to a specific node.
  std::vector<int> BuildNodeQueue(const std::string& node_id,
                                  int max_new = 5) const;

  // Node status queries.
  NodeStatus GetNodeStatus(const std::string& node_id) const;
  double NodeScore(const std::string& node_id) const;
  int NodeCardsSeen(const std::string& node_id) const;

  // Per-card access.
  CardProgress GetProgress(const std::string& card_id) const;

  // Format the skill tree for terminal display.
  std::string FormatTree() const;

  // Format a session summary for the status bar.
  std::string SessionSummary(int session_correct, int session_total) const;

 private:
  std::string ProgressPath() const;
  double CategoryWeight(const std::string& node_id) const;

  // Build queue from a set of eligible card indices.
  std::vector<int> BuildQueueFrom(const std::vector<int>& eligible,
                                  int max_new) const;

  const std::vector<Card>& deck_;
  const std::vector<SkillNode>& tree_;
  std::unordered_map<std::string, CardProgress> data_;

  // card_id -> index in deck_
  std::unordered_map<std::string, int> card_index_;
};

}  // namespace touchstone
