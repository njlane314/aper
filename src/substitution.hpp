#pragma once

#include "penrose.hpp"

#include <vector>

namespace aper::detail {

[[nodiscard]] std::vector<Tile> generate_straight_tiles(Tiling tiling, Seed seed,
                                                        unsigned depth);

} // namespace aper::detail
