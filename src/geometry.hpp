#pragma once

#include <complex>
#include <cstdint>
#include <vector>

namespace aper {

using Point = std::complex<double>;

inline constexpr std::uint8_t maximum_fill = 15;

enum class Shape {
    generic_polygon,
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
    robinson_acute,
    robinson_obtuse,
    ammann_triangle_a,
    ammann_triangle_b,
    pinwheel_triangle,
    equilateral_triangle,
    stampfli_rhomb,
};

struct Tile {
    Shape shape;
    std::vector<Point> vertices;
    // Zero and maximum_fill are the two named scheme colours. Intermediate
    // values form a deterministic gradient between them.
    std::uint8_t fill = 0;
};

} // namespace aper
