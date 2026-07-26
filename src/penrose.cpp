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

struct EdgeKey {
    QuantisedPoint first;
    QuantisedPoint second;

    auto operator<=>(const EdgeKey&) const = default;
};

struct IndexedTriangle {
    EdgeKey seam;
    std::size_t index;
};

struct Seam {
    Point first;
    Point second;
    Point outer;
};

[[nodiscard]] QuantisedPoint quantise(Point point) {
    return {
        std::llround(point.real() * quantisation),
        std::llround(point.imag() * quantisation),
    };
}

[[nodiscard]] EdgeKey edge_key(Point a, Point b) {
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

[[nodiscard]] Point centre(const Tile& tile) {
    Point result{};
    for (const auto vertex : tile.vertices) {
        result += vertex;
    }
    return result / static_cast<double>(tile.vertices.size());
}

[[nodiscard]] Seam seam(const RobinsonTriangle& triangle, Tiling tiling) {
    if (tiling == Tiling::p2) {
        return {triangle.apex, triangle.base_b, triangle.base_a};
    }
    return {triangle.base_a, triangle.base_b, triangle.apex};
}

[[nodiscard]] Shape shape(TriangleKind kind, Tiling tiling) {
    if (tiling == Tiling::p2) {
        return kind == TriangleKind::acute ? Shape::kite : Shape::dart;
    }
    return kind == TriangleKind::acute ? Shape::thin_rhomb : Shape::thick_rhomb;
}

} // namespace

std::vector<RobinsonTriangle> sun_seed() {
    std::vector<RobinsonTriangle> seed;
    seed.reserve(10);

    constexpr double step = std::numbers::pi / 5.0;
    for (unsigned i = 0; i < 10; ++i) {
        auto base_a = std::polar(1.0, static_cast<double>(i) * step);
        auto base_b = std::polar(1.0, static_cast<double>(i + 1) * step);
        if (i % 2 != 0) {
            std::swap(base_a, base_b);
        }
        seed.push_back({TriangleKind::acute, Point{}, base_a, base_b});
    }

    return seed;
}

std::vector<RobinsonTriangle> subdivide(std::span<const RobinsonTriangle> triangles,
                                        Tiling tiling) {
    std::size_t child_count = 0;
    for (const auto& triangle : triangles) {
        if (tiling == Tiling::p2) {
            child_count += triangle.kind == TriangleKind::acute ? 3 : 2;
        } else {
            child_count += triangle.kind == TriangleKind::acute ? 2 : 3;
        }
    }

    std::vector<RobinsonTriangle> children;
    children.reserve(child_count);

    for (const auto& triangle : triangles) {
        const auto& a = triangle.apex;
        const auto& b = triangle.base_a;
        const auto& c = triangle.base_b;

        if (tiling == Tiling::p2) {
            if (triangle.kind == TriangleKind::acute) {
                const auto p = a + (c - a) / phi;
                const auto q = b + (a - b) / phi;
                children.push_back({TriangleKind::acute, b, c, p});
                children.push_back({TriangleKind::acute, b, q, p});
                children.push_back({TriangleKind::obtuse, q, p, a});
                continue;
            }

            const auto p = c + (b - c) / phi;
            children.push_back({TriangleKind::acute, c, p, a});
            children.push_back({TriangleKind::obtuse, p, a, b});
            continue;
        }

        // Ordered Robinson substitutions. The order preserves handedness.
        if (triangle.kind == TriangleKind::acute) {
            const auto p = a + (b - a) / phi;
            children.push_back({TriangleKind::acute, c, p, b});
            children.push_back({TriangleKind::obtuse, p, c, a});
            continue;
        }

        const auto q = b + (a - b) / phi;
        const auto r = b + (c - b) / phi;
        children.push_back({TriangleKind::obtuse, r, c, a});
        children.push_back({TriangleKind::obtuse, q, r, b});
        children.push_back({TriangleKind::acute, r, q, a});
    }

    return children;
}

std::vector<RobinsonTriangle> generate(Tiling tiling, unsigned depth) {
    if (depth > maximum_depth) {
        throw std::invalid_argument("subdivision depth exceeds the supported limit");
    }

    auto triangles = sun_seed();
    for (unsigned generation = 0; generation < depth; ++generation) {
        triangles = subdivide(triangles, tiling);
    }
    return triangles;
}

std::vector<Tile> pair_tiles(std::span<const RobinsonTriangle> triangles,
                             Tiling tiling) {
    std::vector<IndexedTriangle> indexed;
    indexed.reserve(triangles.size());
    for (std::size_t i = 0; i < triangles.size(); ++i) {
        const auto triangle_seam = seam(triangles[i], tiling);
        indexed.push_back({edge_key(triangle_seam.first, triangle_seam.second), i});
    }

    std::sort(indexed.begin(), indexed.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.seam < rhs.seam; });

    std::vector<Tile> tiles;
    tiles.reserve(triangles.size() / 2);

    for (std::size_t first = 0; first < indexed.size();) {
        auto last = first + 1;
        while (last < indexed.size() && indexed[last].seam == indexed[first].seam) {
            ++last;
        }

        const auto multiplicity = last - first;
        if (multiplicity > 2) {
            throw std::runtime_error("more than two triangles share a tile seam");
        }

        if (multiplicity == 2) {
            const auto& first_triangle = triangles[indexed[first].index];
            const auto& second_triangle = triangles[indexed[first + 1].index];
            if (first_triangle.kind != second_triangle.kind) {
                throw std::runtime_error("unlike triangles share a tile seam");
            }

            const auto first_seam = seam(first_triangle, tiling);
            const auto second_seam = seam(second_triangle, tiling);
            if (quantise(first_seam.first) != quantise(second_seam.first) ||
                quantise(first_seam.second) != quantise(second_seam.second)) {
                throw std::runtime_error("triangle seam directions do not match");
            }

            const auto edge = first_seam.second - first_seam.first;
            const auto side_a = cross(edge, first_seam.outer - first_seam.first);
            const auto side_b = cross(edge, second_seam.outer - first_seam.first);
            if (side_a * side_b >= 0.0) {
                throw std::runtime_error("paired triangles lie on the same side");
            }

            if (tiling == Tiling::p3 &&
                std::abs(first_seam.outer + second_seam.outer - first_seam.first -
                         first_seam.second) > geometry_tolerance) {
                throw std::runtime_error(
                    "paired triangles do not form a parallelogram");
            }

            const auto negative = side_a < 0.0 ? first_seam.outer : second_seam.outer;
            const auto positive = side_a > 0.0 ? first_seam.outer : second_seam.outer;
            Tile tile{
                shape(first_triangle.kind, tiling),
                {first_seam.first, negative, first_seam.second, positive},
            };
            canonicalise(tile.vertices);
            tiles.push_back(tile);
        }

        first = last;
    }

    std::sort(tiles.begin(), tiles.end(), [](const Tile& lhs, const Tile& rhs) {
        const auto lhs_centre = quantise(centre(lhs));
        const auto rhs_centre = quantise(centre(rhs));
        return std::tie(lhs_centre.y, lhs_centre.x, lhs.shape) <
               std::tie(rhs_centre.y, rhs_centre.x, rhs.shape);
    });

    return tiles;
}

std::size_t predicted_triangle_count(Tiling tiling, unsigned depth) {
    std::size_t acute = 10;
    std::size_t obtuse = 0;

    for (unsigned generation = 0; generation < depth; ++generation) {
        const auto next_acute =
            tiling == Tiling::p2 ? 2 * acute + obtuse : acute + obtuse;
        const auto next_obtuse =
            tiling == Tiling::p2 ? acute + obtuse : acute + 2 * obtuse;
        acute = next_acute;
        obtuse = next_obtuse;
    }

    return acute + obtuse;
}

} // namespace aper
