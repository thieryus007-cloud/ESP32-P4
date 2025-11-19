#include <array>
#include <cmath>

#include "pack_stats_utils.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

namespace {
bool nearly_equal(float a, float b, float tol = 0.001f)
{
    return std::fabs(a - b) <= tol;
}
}  // namespace

TEST_CASE("compute_extrema returns min max delta avg")
{
    std::array<float, 4> cells{3100.0f, 3180.0f, 3050.0f, 3120.0f};
    auto                 extrema = gui::compute_extrema(std::span<const float>(cells));

    CHECK(extrema.has_cells);
    CHECK(nearly_equal(extrema.min, 3050.0f));
    CHECK(nearly_equal(extrema.max, 3180.0f));
    CHECK(nearly_equal(extrema.delta, 130.0f));
    CHECK(nearly_equal(extrema.avg, (3100.0f + 3180.0f + 3050.0f + 3120.0f) / 4.0f));
}

TEST_CASE("compute_extrema handles empty arrays")
{
    std::array<float, 0> cells{};
    auto                 extrema = gui::compute_extrema(std::span<const float>(cells));

    CHECK(!extrema.has_cells);
    CHECK(nearly_equal(extrema.min, 0.0f));
    CHECK(nearly_equal(extrema.max, 0.0f));
    CHECK(nearly_equal(extrema.delta, 0.0f));
    CHECK(nearly_equal(extrema.avg, 0.0f));
}

TEST_CASE("has_balancing respects cell count and flags")
{
    pack_stats_t stats{};
    stats.cell_count   = 4;
    stats.balancing[1] = true;
    CHECK(gui::has_balancing(stats));

    pack_stats_t clipped{};
    clipped.cell_count   = 2;
    clipped.balancing[3] = true;  // hors plage : doit être ignoré
    CHECK(!gui::has_balancing(clipped));

    clipped.balancing[0] = true;
    CHECK(gui::has_balancing(clipped));
}
