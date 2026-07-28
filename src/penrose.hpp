#pragma once

#include "geometry.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace aper {

inline constexpr unsigned default_depth = 7;
inline constexpr unsigned default_p1_depth = 5;
inline constexpr unsigned minimum_depth = 1;
inline constexpr unsigned maximum_depth = 12;
inline constexpr unsigned maximum_p1_depth = 6;
inline constexpr unsigned default_ammann_beenker_depth = 4;
inline constexpr unsigned maximum_ammann_beenker_depth = 6;
inline constexpr unsigned default_pinwheel_depth = 6;
inline constexpr unsigned maximum_pinwheel_depth = 8;
inline constexpr unsigned default_stampfli_depth = 2;
inline constexpr unsigned maximum_stampfli_depth = 3;
enum class Tiling {
    p1,
    p2,
    p3,
    ammann_beenker,
    pinwheel,
    stampfli,
};

class TilingSystem;

enum class Seed {
    sun,
    star,
    ace,
    deuce,
    jack,
    queen,
    king,
    thin,
    thick,
    pentagon_5,
    pentagon_3,
    pentagon_2,
    diamond,
    boat,
    triangle,
    square,
    rhomb,
    octagon,
    dodecagon,
};

inline constexpr Seed default_seed = Seed::sun;

enum class TriangleKind {
    acute,
    obtuse,
};

struct RobinsonTriangle {
    TriangleKind kind;
    // The ordered base endpoints retain the triangle's handedness.
    Point apex;
    Point base_a;
    Point base_b;
};

std::string_view tiling_name(Tiling tiling);
const TilingSystem& tiling_system(Tiling tiling);
std::string_view seed_name(Seed seed);
bool seed_supported(Tiling tiling, Seed seed);
std::vector<RobinsonTriangle> make_seed(Tiling tiling, Seed seed);
std::vector<RobinsonTriangle> subdivide(std::span<const RobinsonTriangle> triangles,
                                        Tiling tiling);
std::vector<RobinsonTriangle> generate(Tiling tiling, Seed seed, unsigned depth);
std::vector<Tile> pair_tiles(std::span<const RobinsonTriangle> triangles,
                             Tiling tiling);
std::vector<Tile> largest_component(std::span<const Tile> tiles);
std::vector<Tile> generate_tiles(Tiling tiling, Seed seed, unsigned depth);
std::size_t predicted_triangle_count(Tiling tiling, Seed seed, unsigned depth);

} // namespace aper
