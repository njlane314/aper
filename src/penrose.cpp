#include "penrose.hpp"

#include <algorithm>
#include <cmath>
#include <compare>
#include <cstdint>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <string>
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

struct IndexedEdge {
    EdgeKey edge;
    std::size_t tile;
};

struct Seam {
    Point first;
    Point second;
    Point outer;
};

enum class PrototileKind {
    kite,
    dart,
};

struct Prototile {
    std::array<RobinsonTriangle, 2> halves;
    std::array<Point, 4> vertices;
};

struct Placement {
    PrototileKind kind;
    std::size_t role;
    int start;
};

class Components {
  public:
    explicit Components(std::size_t count) : parents_(count), sizes_(count, 1) {
        std::iota(parents_.begin(), parents_.end(), std::size_t{});
    }

    [[nodiscard]] std::size_t root(std::size_t item) {
        while (parents_[item] != item) {
            parents_[item] = parents_[parents_[item]];
            item = parents_[item];
        }
        return item;
    }

    void join(std::size_t first, std::size_t second) {
        first = root(first);
        second = root(second);
        if (first == second) {
            return;
        }
        if (sizes_[first] < sizes_[second]) {
            std::swap(first, second);
        }
        parents_[second] = first;
        sizes_[first] += sizes_[second];
    }

    [[nodiscard]] std::size_t size(std::size_t item) { return sizes_[root(item)]; }

  private:
    std::vector<std::size_t> parents_;
    std::vector<std::size_t> sizes_;
};

[[nodiscard]] Point unit(int step) {
    return std::polar(1.0, static_cast<double>(step) * std::numbers::pi / 5.0);
}

[[nodiscard]] Prototile kite() {
    return {
        std::array{
            RobinsonTriangle{TriangleKind::acute, Point{}, unit(-1), unit(0)},
            RobinsonTriangle{TriangleKind::acute, Point{}, unit(1), unit(0)},
        },
        std::array{Point{}, unit(-1), unit(0), unit(1)},
    };
}

[[nodiscard]] Prototile dart() {
    const auto scale = 1.0 / phi;
    return {
        std::array{
            RobinsonTriangle{TriangleKind::obtuse, Point{}, scale * unit(-3),
                             scale * unit(0)},
            RobinsonTriangle{TriangleKind::obtuse, Point{}, scale * unit(3),
                             scale * unit(0)},
        },
        std::array{Point{}, scale * unit(-3), scale * unit(0), scale * unit(3)},
    };
}

template <std::size_t Size>
[[nodiscard]] std::vector<RobinsonTriangle>
place(std::array<Placement, Size> placements) {
    std::vector<RobinsonTriangle> result;
    result.reserve(2 * placements.size());

    for (const auto placement : placements) {
        const auto prototile = placement.kind == PrototileKind::kite ? kite() : dart();
        const auto origin = prototile.vertices[placement.role];
        const auto edge =
            prototile.vertices[(placement.role + 1) % prototile.vertices.size()] -
            origin;
        const auto rotation = unit(placement.start) * std::abs(edge) / edge;

        for (auto triangle : prototile.halves) {
            triangle.apex = rotation * (triangle.apex - origin);
            triangle.base_a = rotation * (triangle.base_a - origin);
            triangle.base_b = rotation * (triangle.base_b - origin);
            result.push_back(triangle);
        }
    }

    return result;
}

[[nodiscard]] std::vector<RobinsonTriangle> sun_triangles() {
    std::vector<RobinsonTriangle> result;
    result.reserve(10);

    for (int i = 0; i < 10; ++i) {
        auto base_a = unit(i);
        auto base_b = unit(i + 1);
        if (i % 2 != 0) {
            std::swap(base_a, base_b);
        }
        result.push_back({TriangleKind::acute, Point{}, base_a, base_b});
    }

    return result;
}

[[nodiscard]] std::vector<RobinsonTriangle> p2_seed(Seed seed) {
    switch (seed) {
    case Seed::sun:
        return place(std::to_array<Placement>({
            {PrototileKind::kite, 0, 1},
            {PrototileKind::kite, 0, 3},
            {PrototileKind::kite, 0, 5},
            {PrototileKind::kite, 0, 7},
            {PrototileKind::kite, 0, 9},
        }));
    case Seed::star:
        return place(std::to_array<Placement>({
            {PrototileKind::dart, 2, 1},
            {PrototileKind::dart, 2, 3},
            {PrototileKind::dart, 2, 5},
            {PrototileKind::dart, 2, 7},
            {PrototileKind::dart, 2, 9},
        }));
    case Seed::ace:
        return place(std::to_array<Placement>({
            {PrototileKind::dart, 0, 0},
            {PrototileKind::kite, 1, 6},
            {PrototileKind::kite, 3, 8},
        }));
    case Seed::deuce:
        return place(std::to_array<Placement>({
            {PrototileKind::dart, 3, 3},
            {PrototileKind::dart, 1, 4},
            {PrototileKind::kite, 2, 5},
            {PrototileKind::kite, 2, 9},
        }));
    case Seed::jack:
        return place(std::to_array<Placement>({
            {PrototileKind::dart, 3, 2},
            {PrototileKind::kite, 0, 3},
            {PrototileKind::kite, 0, 5},
            {PrototileKind::dart, 1, 7},
            {PrototileKind::kite, 2, 8},
        }));
    case Seed::queen:
        return place(std::to_array<Placement>({
            {PrototileKind::dart, 2, 0},
            {PrototileKind::kite, 3, 2},
            {PrototileKind::kite, 1, 4},
            {PrototileKind::kite, 3, 6},
            {PrototileKind::kite, 1, 8},
        }));
    case Seed::king:
        return place(std::to_array<Placement>({
            {PrototileKind::dart, 2, 1},
            {PrototileKind::dart, 2, 3},
            {PrototileKind::kite, 3, 5},
            {PrototileKind::kite, 1, 7},
            {PrototileKind::dart, 2, 9},
        }));
    case Seed::thin:
    case Seed::thick:
        break;
    }
    throw std::invalid_argument("seed is not available for P2");
}

[[nodiscard]] std::vector<RobinsonTriangle> p3_seed(Seed seed) {
    if (seed == Seed::sun) {
        return sun_triangles();
    }

    if (seed == Seed::star) {
        std::vector<RobinsonTriangle> result;
        result.reserve(10);
        for (int j = 0; j < 5; ++j) {
            const auto apex = unit(2 * j);
            result.push_back(
                {TriangleKind::obtuse, apex, apex - unit(2 * j - 3), Point{}});
            result.push_back(
                {TriangleKind::obtuse, apex, apex - unit(2 * j + 3), Point{}});
        }
        return result;
    }

    if (seed == Seed::thin) {
        const auto first = unit(0);
        const auto second = unit(1);
        return {
            {TriangleKind::acute, Point{}, first, second},
            {TriangleKind::acute, first + second, first, second},
        };
    }

    if (seed == Seed::thick) {
        const auto first = unit(0);
        const auto second = unit(3);
        return {
            {TriangleKind::obtuse, Point{}, first, second},
            {TriangleKind::obtuse, first + second, first, second},
        };
    }

    throw std::invalid_argument("seed is not available for P3");
}

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

std::string_view seed_name(Seed seed) {
    switch (seed) {
    case Seed::sun:
        return "sun";
    case Seed::star:
        return "star";
    case Seed::ace:
        return "ace";
    case Seed::deuce:
        return "deuce";
    case Seed::jack:
        return "jack";
    case Seed::queen:
        return "queen";
    case Seed::king:
        return "king";
    case Seed::thin:
        return "thin";
    case Seed::thick:
        return "thick";
    }
    throw std::invalid_argument("unknown seed");
}

bool seed_supported(Tiling tiling, Seed seed) {
    if (seed == Seed::sun || seed == Seed::star) {
        return true;
    }
    if (tiling == Tiling::p2) {
        return seed == Seed::ace || seed == Seed::deuce || seed == Seed::jack ||
               seed == Seed::queen || seed == Seed::king;
    }
    return seed == Seed::thin || seed == Seed::thick;
}

std::vector<RobinsonTriangle> make_seed(Tiling tiling, Seed seed) {
    if (!seed_supported(tiling, seed)) {
        throw std::invalid_argument(std::string(seed_name(seed)) +
                                    " seed is not available for " +
                                    (tiling == Tiling::p2 ? "P2" : "P3"));
    }
    return tiling == Tiling::p2 ? p2_seed(seed) : p3_seed(seed);
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

std::vector<RobinsonTriangle> generate(Tiling tiling, Seed seed, unsigned depth) {
    if (depth > maximum_depth) {
        throw std::invalid_argument("subdivision depth exceeds the supported limit");
    }

    auto triangles = make_seed(tiling, seed);
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

std::vector<Tile> largest_component(std::span<const Tile> tiles) {
    if (tiles.empty()) {
        return {};
    }

    std::vector<IndexedEdge> indexed;
    indexed.reserve(4 * tiles.size());
    for (std::size_t tile = 0; tile < tiles.size(); ++tile) {
        for (std::size_t vertex = 0; vertex < tiles[tile].vertices.size(); ++vertex) {
            indexed.push_back({
                edge_key(
                    tiles[tile].vertices[vertex],
                    tiles[tile].vertices[(vertex + 1) % tiles[tile].vertices.size()]),
                tile,
            });
        }
    }

    std::sort(indexed.begin(), indexed.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.edge < rhs.edge; });

    Components components(tiles.size());
    for (std::size_t first = 0; first < indexed.size();) {
        auto last = first + 1;
        while (last < indexed.size() && indexed[last].edge == indexed[first].edge) {
            ++last;
        }
        if (last - first > 2) {
            throw std::runtime_error("more than two tiles share an edge");
        }
        if (last - first == 2) {
            components.join(indexed[first].tile, indexed[first + 1].tile);
        }
        first = last;
    }

    auto largest = components.root(0);
    for (std::size_t tile = 1; tile < tiles.size(); ++tile) {
        const auto root = components.root(tile);
        if (components.size(root) > components.size(largest)) {
            largest = root;
        }
    }

    std::vector<Tile> result;
    result.reserve(components.size(largest));
    for (std::size_t tile = 0; tile < tiles.size(); ++tile) {
        if (components.root(tile) == largest) {
            result.push_back(tiles[tile]);
        }
    }
    return result;
}

std::size_t predicted_triangle_count(Tiling tiling, Seed seed, unsigned depth) {
    const auto triangles = make_seed(tiling, seed);
    std::size_t acute = static_cast<std::size_t>(
        std::count_if(triangles.begin(), triangles.end(), [](const auto& triangle) {
            return triangle.kind == TriangleKind::acute;
        }));
    std::size_t obtuse = triangles.size() - acute;

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
