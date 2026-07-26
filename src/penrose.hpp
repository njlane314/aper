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

enum class Kind {
    thin,
    thick,
};

struct HalfRhomb {
    Kind kind;
    // The base is the diagonal shared by the two halves of a complete rhomb.
    Point apex;
    Point base_a;
    Point base_b;
};

struct Rhomb {
    Kind kind;
    std::array<Point, 4> vertices;
};

[[nodiscard]] std::vector<HalfRhomb> sun_seed();
[[nodiscard]] std::vector<HalfRhomb> subdivide(std::span<const HalfRhomb> halves);
[[nodiscard]] std::vector<HalfRhomb> generate(unsigned depth);
[[nodiscard]] std::vector<Rhomb> pair_rhombs(std::span<const HalfRhomb> halves);
[[nodiscard]] std::size_t predicted_half_count(unsigned depth);

} // namespace aper
