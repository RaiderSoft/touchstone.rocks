#include "nodes.h"

namespace touchstone {

static const std::vector<SkillNode> kSkillTree = {
    {
        "capture",
        "Capture",
        "Find the move that captures opponent stones.",
        {},  // no prerequisites - always unlocked
        {"cap-01", "cap-02"},
        0.8,
    },
    {
        "defend",
        "Defend",
        "Save your stones from capture.",
        {"capture"},
        {"def-01"},
        0.8,
    },
    {
        "life-death",
        "Life & Death",
        "Kill opponent groups or make your groups live.",
        {"defend"},
        {"life-01"},
        0.8,
    },
    {
        "tesuji",
        "Tesuji",
        "Find the clever tactical move.",
        {"life-death"},
        {"tesuji-01"},
        0.8,
    },
};

const std::vector<SkillNode>& SkillTree() { return kSkillTree; }

}  // namespace touchstone
