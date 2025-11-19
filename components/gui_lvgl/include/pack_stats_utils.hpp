#pragma once

#include <span>

#include "event_types.h"

namespace gui {

struct cell_extrema {
    float min{0.0f};
    float max{0.0f};
    float delta{0.0f};
    float avg{0.0f};
    bool  has_cells{false};
};

std::span<const float> cell_values(const pack_stats_t &stats);
std::span<const bool> balancing_states(const pack_stats_t &stats);

cell_extrema compute_extrema(std::span<const float> cells);

bool has_balancing(std::span<const bool> balancing);
bool has_balancing(const pack_stats_t &stats);

}  // namespace gui
