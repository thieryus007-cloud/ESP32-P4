#include "pack_stats_utils.hpp"

#include <algorithm>
#include <numeric>

namespace gui {

namespace {
constexpr std::size_t clamp_cell_count(uint8_t count)
{
    return static_cast<std::size_t>(std::min<uint8_t>(count, PACK_MAX_CELLS));
}
}  // namespace

std::span<const float> cell_values(const pack_stats_t &stats)
{
    const auto count = clamp_cell_count(stats.cell_count);
    return std::span<const float>(stats.cells, count);
}

std::span<const bool> balancing_states(const pack_stats_t &stats)
{
    const auto count = clamp_cell_count(stats.cell_count);
    return std::span<const bool>(stats.balancing, count);
}

cell_extrema compute_extrema(std::span<const float> cells)
{
    cell_extrema extrema{};
    if (cells.empty()) {
        return extrema;
    }

    const auto [min_it, max_it] = std::minmax_element(cells.begin(), cells.end());
    extrema.min       = *min_it;
    extrema.max       = *max_it;
    extrema.delta     = extrema.max - extrema.min;
    extrema.avg       = std::accumulate(cells.begin(), cells.end(), 0.0f) /
                  static_cast<float>(cells.size());
    extrema.has_cells = true;
    return extrema;
}

bool has_balancing(std::span<const bool> balancing)
{
    return std::any_of(balancing.begin(), balancing.end(), [](bool value) { return value; });
}

bool has_balancing(const pack_stats_t &stats)
{
    return has_balancing(balancing_states(stats));
}

}  // namespace gui
