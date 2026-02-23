#pragma once

#include <string>
#include <vector>

namespace touchstone {

struct SkillNode {
  std::string id;
  std::string name;
  std::string description;
  std::vector<std::string> requires;    // prerequisite node IDs
  std::vector<std::string> card_ids;    // cards belonging to this node
  double mastery_threshold = 0.8;       // score needed to "master" this node
};

// Returns the full skill tree (ordered by dependency).
const std::vector<SkillNode>& SkillTree();

}  // namespace touchstone
