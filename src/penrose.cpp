#include "penrose.hpp"

#include <algorithm>
#include <cmath>
#include <compare>
#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace aper {
namespace {

constexpr double phi = std::numbers::phi;
// All geometry stays inside the unit seed, including at the maximum depth.
constexpr double quantisation = 1.0e10;
constexpr double geometry_tolerance = 1.0e-8;

struct QuantisedPoint {
    std::int64_t x;
    std::int64_t y;

    auto operator<=>(const QuantisedPoint&) const = default;
};

struct BaseKey {
    QuantisedPoint first;
    QuantisedPoint second;

    auto operator<=>(const BaseKey&) const = default;
};

struct IndexedHalf {
    BaseKey base;
    std::size_t index;
};

[[nodiscard]] QuantisedPoint quantise(Point point) {
    return {
        std::llround(point.real() * quantisation),
        std::llround(point.imag() * quantisation),
    };
}

[[nodiscard]] BaseKey base_key(Point a, Point b) {
    auto first = quantise(a);
    auto second = quantise(b);
    if (second < first) {
        std::swap(first, second);
    }
    return {first, second};
}

[[nodiscard]] double cross(Point a, Point b) {
    return a.real() * b.imag() - a.imag() * b.real();
}

[[nodiscard]] double signed_area(const std::array<Point, 4>& vertices) {
    double twice_area = 0.0;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        twice_area += cross(vertices[i], vertices[(i + 1) % vertices.size()]);
    }
    return twice_area / 2.0;
}

void canonicalise(std::array<Point, 4>& vertices) {
    if (signed_area(vertices) < 0.0) {
        std::reverse(vertices.begin(), vertices.end());
    }

    const auto first =
        std::min_element(vertices.begin(), vertices.end(), [](Point lhs, Point rhs) {
            return std::tuple{lhs.real(), lhs.imag()} <
                   std::tuple{rhs.real(), rhs.imag()};
        });
    std::rotate(vertices.begin(), first, vertices.end());
}

[[nodiscard]] Point centre(const Rhomb& rhomb) {
    Point result{};
    for (const auto vertex : rhomb.vertices) {
        result += vertex;
    }
    return result / static_cast<double>(rhomb.vertices.size());
}

} // namespace

std::vector<HalfRhomb> sun_seed() {
    std::vector<HalfRhomb> seed;
    seed.reserve(10);

    constexpr double step = std::numbers::pi / 5.0;
    for (unsigned i = 0; i < 10; ++i) {
        auto base_a = std::polar(1.0, static_cast<double>(i) * step);
        auto base_b = std::polar(1.0, static_cast<double>(i + 1) * step);
        if (i % 2 != 0) {
            std::swap(base_a, base_b);
        }
        seed.push_back({Kind::thin, Point{}, base_a, base_b});
    }

    return seed;
}

std::vector<HalfRhomb> subdivide(std::span<const HalfRhomb> halves) {
    std::size_t child_count = 0;
    for (const auto& half : halves) {
        child_count += half.kind == Kind::thin ? 2 : 3;
    }

    std::vector<HalfRhomb> children;
    children.reserve(child_count);

    for (const auto& half : halves) {
        const auto& a = half.apex;
        const auto& b = half.base_a;
        const auto& c = half.base_b;

        // Ordered Robinson substitutions. The order preserves handedness.
        if (half.kind == Kind::thin) {
            const auto p = a + (b - a) / phi;
            children.push_back({Kind::thin, c, p, b});
            children.push_back({Kind::thick, p, c, a});
            continue;
        }

        const auto q = b + (a - b) / phi;
        const auto r = b + (c - b) / phi;
        children.push_back({Kind::thick, r, c, a});
        children.push_back({Kind::thick, q, r, b});
        children.push_back({Kind::thin, r, q, a});
    }

    return children;
}

std::vector<HalfRhomb> generate(unsigned depth) {
    if (depth > maximum_depth) {
        throw std::invalid_argument("subdivision depth exceeds the supported limit");
    }

    auto halves = sun_seed();
    for (unsigned generation = 0; generation < depth; ++generation) {
        halves = subdivide(halves);
    }
    return halves;
}

std::vector<Rhomb> pair_rhombs(std::span<const HalfRhomb> halves) {
    std::vector<IndexedHalf> indexed;
    indexed.reserve(halves.size());
    for (std::size_t i = 0; i < halves.size(); ++i) {
        indexed.push_back({base_key(halves[i].base_a, halves[i].base_b), i});
    }

    std::sort(indexed.begin(), indexed.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.base < rhs.base; });

    std::vector<Rhomb> rhombs;
    rhombs.reserve(halves.size() / 2);

    for (std::size_t first = 0; first < indexed.size();) {
        auto last = first + 1;
        while (last < indexed.size() && indexed[last].base == indexed[first].base) {
            ++last;
        }

        const auto multiplicity = last - first;
        if (multiplicity > 2) {
            throw std::runtime_error("more than two half-rhombs share a base");
        }

        if (multiplicity == 2) {
            const auto& left = halves[indexed[first].index];
            const auto& right = halves[indexed[first + 1].index];
            if (left.kind != right.kind) {
                throw std::runtime_error("unlike half-rhombs share a base");
            }

            const auto base = left.base_b - left.base_a;
            const auto side_a = cross(base, left.apex - left.base_a);
            const auto side_b = cross(base, right.apex - left.base_a);
            if (side_a * side_b >= 0.0) {
                throw std::runtime_error("paired half-rhombs lie on the same side");
            }

            if (std::abs(left.apex + right.apex - left.base_a - left.base_b) >
                geometry_tolerance) {
                throw std::runtime_error(
                    "paired half-rhombs do not form a parallelogram");
            }

            Rhomb rhomb{
                left.kind,
                {left.apex, left.base_a, right.apex, left.base_b},
            };
            canonicalise(rhomb.vertices);
            rhombs.push_back(rhomb);
        }

        first = last;
    }

    std::sort(rhombs.begin(), rhombs.end(), [](const Rhomb& lhs, const Rhomb& rhs) {
        const auto lhs_centre = quantise(centre(lhs));
        const auto rhs_centre = quantise(centre(rhs));
        return std::tie(lhs_centre.y, lhs_centre.x, lhs.kind) <
               std::tie(rhs_centre.y, rhs_centre.x, rhs.kind);
    });

    return rhombs;
}

std::size_t predicted_half_count(unsigned depth) {
    std::size_t thin = 10;
    std::size_t thick = 0;

    for (unsigned generation = 0; generation < depth; ++generation) {
        const auto next_thin = thin + thick;
        const auto next_thick = thin + 2 * thick;
        thin = next_thin;
        thick = next_thick;
    }

    return thin + thick;
}

} // namespace aper
