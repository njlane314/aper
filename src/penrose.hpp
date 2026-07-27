#pragma once

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace aper {

using Point = std::complex<double>;

inline constexpr unsigned default_depth = 7;
inline constexpr unsigned default_p1_depth = 4;
inline constexpr unsigned minimum_depth = 1;
inline constexpr unsigned maximum_depth = 12;
inline constexpr unsigned maximum_p1_depth = 6;
inline constexpr unsigned default_ammann_beenker_depth = 4;
inline constexpr unsigned maximum_ammann_beenker_depth = 6;
inline constexpr unsigned default_pinwheel_depth = 6;
inline constexpr unsigned maximum_pinwheel_depth = 8;
inline constexpr unsigned default_stampfli_depth = 2;
inline constexpr unsigned maximum_stampfli_depth = 3;
inline constexpr std::uint8_t maximum_fill = 15;

enum class Tiling {
    p1,
    p2,
    p3,
    ammann_beenker,
    pinwheel,
    stampfli,
};

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

enum class Shape {
    pentagon_5,
    pentagon_3,
    pentagon_2,
    diamond,
    boat,
    star,
    kite,
    dart,
    thin_rhomb,
    thick_rhomb,
    square,
    ammann_rhomb,
    pinwheel_triangle,
    equilateral_triangle,
    stampfli_rhomb,
};

struct RobinsonTriangle {
    TriangleKind kind;
    // The ordered base endpoints retain the triangle's handedness.
    Point apex;
    Point base_a;
    Point base_b;
};

struct Tile {
    Shape shape;
    std::vector<Point> vertices;
    // Zero and maximum_fill are the two named scheme colours. Intermediate
    // values form a deterministic gradient between them.
    std::uint8_t fill = 0;
};

[[nodiscard]] std::string_view tiling_name(Tiling tiling);
[[nodiscard]] std::string_view seed_name(Seed seed);
[[nodiscard]] bool seed_supported(Tiling tiling, Seed seed);
[[nodiscard]] std::vector<RobinsonTriangle> make_seed(Tiling tiling, Seed seed);
[[nodiscard]] std::vector<RobinsonTriangle>
subdivide(std::span<const RobinsonTriangle> triangles, Tiling tiling);
[[nodiscard]] std::vector<RobinsonTriangle> generate(Tiling tiling, Seed seed,
                                                     unsigned depth);
[[nodiscard]] std::vector<Tile> pair_tiles(std::span<const RobinsonTriangle> triangles,
                                           Tiling tiling);
[[nodiscard]] std::vector<Tile> largest_component(std::span<const Tile> tiles);
[[nodiscard]] std::vector<Tile> generate_tiles(Tiling tiling, Seed seed,
                                               unsigned depth);
[[nodiscard]] std::size_t predicted_triangle_count(Tiling tiling, Seed seed,
                                                   unsigned depth);

} // namespace aper
