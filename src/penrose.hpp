#pragma once

#include <array>
#include <complex>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace aper {

using Point = std::complex<double>;

inline constexpr unsigned default_depth = 7;
inline constexpr unsigned minimum_depth = 1;
inline constexpr unsigned maximum_depth = 12;

enum class Tiling {
    p2,
    p3,
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
};

inline constexpr Seed default_seed = Seed::sun;

enum class TriangleKind {
    acute,
    obtuse,
};

enum class Shape {
    kite,
    dart,
    thin_rhomb,
    thick_rhomb,
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
    std::array<Point, 4> vertices;
};

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
[[nodiscard]] std::size_t predicted_triangle_count(Tiling tiling, Seed seed,
                                                   unsigned depth);

} // namespace aper
