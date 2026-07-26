#pragma once

#include <array>
#include <complex>
#include <cstddef>
#include <span>
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

[[nodiscard]] std::vector<RobinsonTriangle> sun_seed();
[[nodiscard]] std::vector<RobinsonTriangle>
subdivide(std::span<const RobinsonTriangle> triangles, Tiling tiling);
[[nodiscard]] std::vector<RobinsonTriangle> generate(Tiling tiling, unsigned depth);
[[nodiscard]] std::vector<Tile> pair_tiles(std::span<const RobinsonTriangle> triangles,
                                           Tiling tiling);
[[nodiscard]] std::size_t predicted_triangle_count(Tiling tiling, unsigned depth);

} // namespace aper
