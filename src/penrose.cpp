#include "penrose.hpp"
#include "system.hpp"

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
// Rendered geometry stays on the scale of its bounded seed, even at maximum depth.
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

struct P1Prototile {
    Shape shape;
    Point origin;
    // Orientations are exact multiples of pi/5. Keeping them integral avoids
    // accumulating angular drift over repeated inflation.
    int orientation;
};

class Components {
  public:
    explicit Components(std::size_t count) : parents_(count), sizes_(count, 1) {
        std::iota(parents_.begin(), parents_.end(), std::size_t{});
    }

    std::size_t root(std::size_t item) {
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

    std::size_t size(std::size_t item) { return sizes_[root(item)]; }

  private:
    std::vector<std::size_t> parents_;
    std::vector<std::size_t> sizes_;
};

Point unit(int step) {
    return std::polar(1.0, static_cast<double>(step) * std::numbers::pi / 5.0);
}

int normalise_orientation(int orientation) {
    orientation %= 10;
    return orientation < 0 ? orientation + 10 : orientation;
}

Point p1_ray(int orientation, int tenth_turns, double length) {
    const auto angle =
        static_cast<double>(2 * orientation + tenth_turns) * std::numbers::pi / 10.0;
    return std::polar(length, angle);
}

std::vector<Point> p1_vertices(const P1Prototile& tile, double scale = 1.0) {
    const auto point = [&](int tenth_turns, double length) {
        return scale * (tile.origin + p1_ray(tile.orientation, tenth_turns, length));
    };
    const auto origin = scale * tile.origin;

    switch (tile.shape) {
    case Shape::pentagon_5:
    case Shape::pentagon_3:
    case Shape::pentagon_2:
        return {origin, point(3, 1.0), point(1, phi), point(-1, phi), point(-3, 1.0)};
    case Shape::diamond:
        return {origin, point(1, 1.0),
                point(0, 2.0 * std::cos(std::numbers::pi / 10.0)), point(-1, 1.0)};
    case Shape::boat:
        return {origin,         point(1, 1.0),  point(3, phi), point(1, phi),
                point(-1, phi), point(-3, phi), point(-1, 1.0)};
    case Shape::star:
        return {origin,
                point(1, 1.0),
                point(3, phi),
                point(1, phi),
                point(1, phi * phi),
                point(0, 2.0 * std::cos(std::numbers::pi / 10.0)),
                point(-1, phi * phi),
                point(-1, phi),
                point(-3, phi),
                point(-1, 1.0)};
    case Shape::generic_polygon:
    case Shape::kite:
    case Shape::dart:
    case Shape::thin_rhomb:
    case Shape::thick_rhomb:
    case Shape::square:
    case Shape::ammann_rhomb:
    case Shape::robinson_acute:
    case Shape::robinson_obtuse:
    case Shape::ammann_triangle_a:
    case Shape::ammann_triangle_b:
    case Shape::pinwheel_triangle:
    case Shape::equilateral_triangle:
    case Shape::stampfli_rhomb:
        break;
    }
    throw std::invalid_argument("shape is not a P1 prototile");
}

P1Prototile p1_tile(Shape shape, Point origin, int orientation) {
    return {shape, origin, normalise_orientation(orientation)};
}

Point p1_vertex(const P1Prototile& tile, std::size_t vertex, double scale = 1.0) {
    return p1_vertices(tile, scale).at(vertex);
}

Point opposite_side(const P1Prototile& pentagon) {
    const auto span = std::sqrt(5.0 + 2.0 * std::sqrt(5.0));
    return pentagon.origin + p1_ray(pentagon.orientation, 0, span);
}

std::vector<P1Prototile> subdivide_p1(const P1Prototile& tile) {
    const auto inflated = p1_vertices(tile, phi * phi);
    std::vector<P1Prototile> children;

    if (tile.shape == Shape::star || tile.shape == Shape::boat) {
        P1Prototile inner_star;
        int first = 1;
        int last = 5;
        if (tile.shape == Shape::star) {
            inner_star = p1_tile(Shape::star, inflated[5], tile.orientation + 5);
            children.reserve(11);
        } else {
            const auto midpoint = (inflated[3] + inflated[4]) / 2.0;
            const auto origin = midpoint + p1_ray(tile.orientation, 0,
                                                  std::cos(std::numbers::pi / 10.0));
            inner_star = p1_tile(Shape::star, origin, tile.orientation + 5);
            children.reserve(7);
            first = 2;
            last = 4;
        }

        children.push_back(inner_star);
        const auto star_vertices = p1_vertices(inner_star);
        std::vector<P1Prototile> pentagons;
        pentagons.reserve(static_cast<std::size_t>(last - first + 1));
        for (int i = first; i <= last; ++i) {
            pentagons.push_back(p1_tile(
                Shape::pentagon_2, star_vertices[static_cast<std::size_t>(2 * i - 1)],
                inner_star.orientation + 4 * (2 * i - 1)));
        }
        children.insert(children.end(), pentagons.begin(), pentagons.end());
        for (const auto& pentagon : pentagons) {
            children.push_back(p1_tile(Shape::boat, opposite_side(pentagon),
                                       pentagon.orientation + 5));
        }
        return children;
    }

    if (tile.shape == Shape::diamond) {
        children.reserve(3);
        const auto inner_star = p1_tile(Shape::star, inflated[0], tile.orientation);
        const auto pentagon =
            p1_tile(Shape::pentagon_2, p1_vertex(inner_star, 5), tile.orientation);
        children.push_back(inner_star);
        children.push_back(pentagon);
        children.push_back(
            p1_tile(Shape::boat, opposite_side(pentagon), tile.orientation + 5));
        return children;
    }

    if (tile.shape != Shape::pentagon_5 && tile.shape != Shape::pentagon_3 &&
        tile.shape != Shape::pentagon_2) {
        throw std::invalid_argument("shape is not a P1 prototile");
    }

    std::array<P1Prototile, 5> outer;
    for (std::size_t i = 0; i < outer.size(); ++i) {
        outer[i] = p1_tile(Shape::pentagon_3, inflated[i],
                           tile.orientation - 2 * static_cast<int>(i));
    }
    const auto inner =
        p1_tile(Shape::pentagon_5, p1_vertex(outer[2], 2), tile.orientation + 5);

    if (tile.shape == Shape::pentagon_5) {
        children.assign(outer.begin(), outer.end());
        children.push_back(inner);
        return children;
    }

    if (tile.shape == Shape::pentagon_3) {
        children.reserve(7);
        children.push_back(outer[0]);
        children.push_back(outer[1]);
        children.push_back(outer[4]);
        children.push_back(p1_tile(Shape::pentagon_2, p1_vertex(outer[2], 4),
                                   outer[2].orientation + 2));
        children.push_back(p1_tile(Shape::pentagon_2, p1_vertex(outer[3], 1),
                                   outer[3].orientation - 2));
        children.push_back(inner);
        const auto dummy = p1_tile(Shape::diamond, inner.origin, inner.orientation + 5);
        children.push_back(
            p1_tile(Shape::diamond, p1_vertex(dummy, 2), inner.orientation));
        return children;
    }

    children.reserve(8);
    children.push_back(outer[0]);
    children.push_back(
        p1_tile(Shape::pentagon_2, p1_vertex(outer[1], 4), tile.orientation));
    children.push_back(
        p1_tile(Shape::pentagon_2, p1_vertex(outer[4], 1), tile.orientation));
    children.push_back(
        p1_tile(Shape::pentagon_2, p1_vertex(outer[2], 1), tile.orientation + 4));
    children.push_back(
        p1_tile(Shape::pentagon_2, p1_vertex(outer[3], 4), tile.orientation - 4));
    children.push_back(inner);

    const auto first_dummy =
        p1_tile(Shape::diamond, p1_vertex(outer[1], 2), outer[1].orientation + 4);
    const auto second_dummy =
        p1_tile(Shape::diamond, p1_vertex(outer[3], 2), outer[1].orientation);
    children.push_back(p1_tile(Shape::diamond, p1_vertex(first_dummy, 2),
                               first_dummy.orientation + 5));
    children.push_back(p1_tile(Shape::diamond, p1_vertex(second_dummy, 2),
                               second_dummy.orientation + 5));
    return children;
}

Prototile kite() {
    return {
        std::array{
            RobinsonTriangle{TriangleKind::acute, Point{}, unit(-1), unit(0)},
            RobinsonTriangle{TriangleKind::acute, Point{}, unit(1), unit(0)},
        },
        std::array{Point{}, unit(-1), unit(0), unit(1)},
    };
}

Prototile dart() {
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
std::vector<RobinsonTriangle> place(std::array<Placement, Size> placements) {
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

std::vector<RobinsonTriangle> sun_triangles() {
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

std::vector<RobinsonTriangle> p2_seed(Seed seed) {
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
    case Seed::pentagon_5:
    case Seed::pentagon_3:
    case Seed::pentagon_2:
    case Seed::diamond:
    case Seed::boat:
    case Seed::triangle:
    case Seed::square:
    case Seed::rhomb:
    case Seed::octagon:
    case Seed::dodecagon:
        break;
    }
    throw std::invalid_argument("seed is not available for P2");
}

std::vector<RobinsonTriangle> p3_seed(Seed seed) {
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

QuantisedPoint quantise(Point point) {
    return {
        std::llround(point.real() * quantisation),
        std::llround(point.imag() * quantisation),
    };
}

EdgeKey edge_key(Point a, Point b) {
    auto first = quantise(a);
    auto second = quantise(b);
    if (second < first) {
        std::swap(first, second);
    }
    return {first, second};
}

double cross(Point a, Point b) { return a.real() * b.imag() - a.imag() * b.real(); }

double signed_area(std::span<const Point> vertices) {
    double twice_area = 0.0;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        twice_area += cross(vertices[i], vertices[(i + 1) % vertices.size()]);
    }
    return twice_area / 2.0;
}

void canonicalise(std::vector<Point>& vertices) {
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

Point centre(const Tile& tile) {
    Point result{};
    for (const auto vertex : tile.vertices) {
        result += vertex;
    }
    return result / static_cast<double>(tile.vertices.size());
}

void sort_tiles(std::vector<Tile>& tiles) {
    std::sort(tiles.begin(), tiles.end(), [](const Tile& lhs, const Tile& rhs) {
        const auto lhs_centre = quantise(centre(lhs));
        const auto rhs_centre = quantise(centre(rhs));
        return std::tie(lhs_centre.y, lhs_centre.x, lhs.shape) <
               std::tie(rhs_centre.y, rhs_centre.x, rhs.shape);
    });
}

Seam seam(const RobinsonTriangle& triangle, Tiling tiling) {
    if (tiling == Tiling::p2) {
        return {triangle.apex, triangle.base_b, triangle.base_a};
    }
    if (tiling == Tiling::p3) {
        return {triangle.base_a, triangle.base_b, triangle.apex};
    }
    throw std::invalid_argument("tiling does not use Robinson seams");
}

Shape shape(TriangleKind kind, Tiling tiling) {
    if (tiling == Tiling::p2) {
        return kind == TriangleKind::acute ? Shape::kite : Shape::dart;
    }
    if (tiling == Tiling::p3) {
        return kind == TriangleKind::acute ? Shape::thin_rhomb : Shape::thick_rhomb;
    }
    throw std::invalid_argument("tiling does not use Robinson-triangle pairs");
}

class RobinsonProjector final : public PatchProjector {
  public:
    explicit RobinsonProjector(Tiling tiling) : tiling_(tiling) {}

    std::vector<Tile> project(const TilingSystem& system,
                              const Patch& patch) const override {
        std::vector<RobinsonTriangle> triangles;
        triangles.reserve(patch.size());
        for (const auto& placement : patch.placements()) {
            const auto& prototile = system.prototile(placement.prototile);
            if (prototile.boundary.size() != 3 ||
                (prototile.shape != Shape::robinson_acute &&
                 prototile.shape != Shape::robinson_obtuse)) {
                throw std::runtime_error(
                    "non-Robinson prototile in Robinson projection");
            }
            triangles.push_back({
                prototile.shape == Shape::robinson_acute ? TriangleKind::acute
                                                         : TriangleKind::obtuse,
                placement.pose.apply(prototile.boundary[0]),
                placement.pose.apply(prototile.boundary[1]),
                placement.pose.apply(prototile.boundary[2]),
            });
        }
        return largest_component(pair_tiles(triangles, tiling_));
    }

  private:
    Tiling tiling_;
};

PrototileId p1_id(Shape shape) {
    switch (shape) {
    case Shape::star:
        return 0;
    case Shape::pentagon_5:
        return 1;
    case Shape::boat:
        return 2;
    case Shape::pentagon_3:
        return 3;
    case Shape::pentagon_2:
        return 4;
    case Shape::diamond:
        return 5;
    case Shape::generic_polygon:
    case Shape::kite:
    case Shape::dart:
    case Shape::thin_rhomb:
    case Shape::thick_rhomb:
    case Shape::square:
    case Shape::ammann_rhomb:
    case Shape::robinson_acute:
    case Shape::robinson_obtuse:
    case Shape::ammann_triangle_a:
    case Shape::ammann_triangle_b:
    case Shape::pinwheel_triangle:
    case Shape::equilateral_triangle:
    case Shape::stampfli_rhomb:
        break;
    }
    throw std::invalid_argument("shape is not a P1 prototile");
}

aper::Placement triangle_placement(const aper::Prototile& prototile,
                                   const RobinsonTriangle& triangle) {
    const std::array target{triangle.apex, triangle.base_a, triangle.base_b};
    for (const auto reflected : {false, true}) {
        std::array<Point, 3> source{};
        for (std::size_t i = 0; i < source.size(); ++i) {
            source[i] =
                reflected ? std::conj(prototile.boundary[i]) : prototile.boundary[i];
        }
        const auto multiplier = (target[1] - target[0]) / (source[1] - source[0]);
        const auto translation = target[0] - multiplier * source[0];
        if (std::abs(translation + multiplier * source[2] - target[2]) <
            geometry_tolerance) {
            return {prototile.id, Similarity{translation, multiplier, reflected}};
        }
    }
    throw std::runtime_error("triangle is not similar to its prototile");
}

TilingSystem make_p1_system() {
    const std::array shapes{
        Shape::star,       Shape::pentagon_5, Shape::boat,
        Shape::pentagon_3, Shape::pentagon_2, Shape::diamond,
    };
    const std::array names{
        std::string_view{"star"},       std::string_view{"pentagon-5"},
        std::string_view{"boat"},       std::string_view{"pentagon-3"},
        std::string_view{"pentagon-2"}, std::string_view{"diamond"},
    };

    std::vector<aper::Prototile> prototiles;
    std::vector<RuleEntry> rules;
    prototiles.reserve(shapes.size());
    rules.reserve(shapes.size());
    for (std::size_t id = 0; id < shapes.size(); ++id) {
        const auto parent = p1_tile(shapes[id], Point{}, 0);
        const auto fill = shapes[id] == Shape::star || shapes[id] == Shape::boat ||
                                  shapes[id] == Shape::diamond
                              ? std::uint8_t{0}
                              : maximum_fill;
        prototiles.push_back(
            {id, std::string(names[id]), shapes[id], p1_vertices(parent), fill});

        std::vector<aper::Placement> children;
        for (const auto& child_tile : subdivide_p1(parent)) {
            const auto scale = 1.0 / (phi * phi);
            children.push_back({
                p1_id(child_tile.shape),
                Similarity{
                    scale * child_tile.origin,
                    std::polar(scale, static_cast<double>(child_tile.orientation) *
                                          std::numbers::pi / 5.0),
                },
            });
        }
        rules.push_back({id, Patch{std::move(children)}});
    }

    const auto seed = [](std::string name, Shape shape) {
        return SeedPatch{std::move(name), Patch{{aper::Placement{p1_id(shape), {}}}},
                         1};
    };
    std::vector<SeedPatch> seeds;
    seeds.push_back(seed("pentagon-5", Shape::pentagon_5));
    seeds.push_back(seed("pentagon-3", Shape::pentagon_3));
    seeds.push_back(seed("pentagon-2", Shape::pentagon_2));
    seeds.push_back(seed("diamond", Shape::diamond));
    seeds.push_back(seed("boat", Shape::boat));
    seeds.push_back(seed("star", Shape::star));

    return {
        {"p1",
         "P1",
         {"pentagon-boat-star"},
         "pentagon-5",
         {minimum_depth, default_p1_depth, maximum_p1_depth}},
        std::move(prototiles),
        SubstitutionRule{phi * phi, std::move(rules)},
        std::move(seeds),
        identity_projector(),
    };
}

TilingSystem make_robinson_system(Tiling tiling) {
    RobinsonTriangle acute;
    RobinsonTriangle obtuse;
    if (tiling == Tiling::p2) {
        acute = kite().halves[0];
        obtuse = dart().halves[0];
    } else if (tiling == Tiling::p3) {
        acute = {TriangleKind::acute, Point{}, unit(0), unit(1)};
        obtuse = {TriangleKind::obtuse, Point{}, unit(0), unit(3)};
    } else {
        throw std::invalid_argument("tiling does not use Robinson triangles");
    }

    std::vector<aper::Prototile> prototiles{
        {0,
         "acute",
         Shape::robinson_acute,
         {acute.apex, acute.base_a, acute.base_b},
         0},
        {1,
         "obtuse",
         Shape::robinson_obtuse,
         {obtuse.apex, obtuse.base_a, obtuse.base_b},
         maximum_fill},
    };
    const auto rule_for = [&](const RobinsonTriangle& parent) {
        const std::array parents{parent};
        const auto triangles = subdivide(parents, tiling);
        std::vector<aper::Placement> placements;
        placements.reserve(triangles.size());
        for (const auto& triangle : triangles) {
            const auto id = triangle.kind == TriangleKind::acute ? 0U : 1U;
            placements.push_back(triangle_placement(prototiles[id], triangle));
        }
        return Patch{std::move(placements)};
    };
    std::vector<RuleEntry> rules{
        {0, rule_for(acute)},
        {1, rule_for(obtuse)},
    };

    const auto make_seed_patch = [&](std::string name, Seed seed,
                                     unsigned minimum = 1) {
        std::vector<aper::Placement> placements;
        for (const auto& triangle :
             (tiling == Tiling::p2 ? p2_seed(seed) : p3_seed(seed))) {
            const auto id = triangle.kind == TriangleKind::acute ? 0U : 1U;
            placements.push_back(triangle_placement(prototiles[id], triangle));
        }
        return SeedPatch{std::move(name), Patch{std::move(placements)}, minimum};
    };

    std::vector<SeedPatch> seeds;
    if (tiling == Tiling::p2) {
        seeds.push_back(make_seed_patch("sun", Seed::sun));
        seeds.push_back(make_seed_patch("star", Seed::star));
        seeds.push_back(make_seed_patch("ace", Seed::ace));
        seeds.push_back(make_seed_patch("deuce", Seed::deuce));
        seeds.push_back(make_seed_patch("jack", Seed::jack));
        seeds.push_back(make_seed_patch("queen", Seed::queen));
        seeds.push_back(make_seed_patch("king", Seed::king));
    } else {
        seeds.push_back(make_seed_patch("sun", Seed::sun));
        seeds.push_back(make_seed_patch("star", Seed::star));
        seeds.push_back(make_seed_patch("thin", Seed::thin, 2));
        seeds.push_back(make_seed_patch("thick", Seed::thick));
    }

    const auto p2 = tiling == Tiling::p2;
    return {
        {p2 ? "p2" : "p3",
         p2 ? "P2" : "P3",
         {p2 ? "kite-dart" : "rhomb"},
         "sun",
         {minimum_depth, default_depth, maximum_depth}},
        std::move(prototiles),
        SubstitutionRule{phi, std::move(rules)},
        std::move(seeds),
        std::make_shared<const RobinsonProjector>(tiling),
    };
}

} // namespace

const TilingSystem& tiling_system(Tiling tiling) {
    switch (tiling) {
    case Tiling::p1:
        return tiling_catalogue().get("p1");
    case Tiling::p2:
        return tiling_catalogue().get("p2");
    case Tiling::p3:
        return tiling_catalogue().get("p3");
    case Tiling::ammann_beenker:
        return tiling_catalogue().get("ammann-beenker");
    case Tiling::pinwheel:
        return tiling_catalogue().get("pinwheel");
    case Tiling::stampfli:
        return tiling_catalogue().get("stampfli");
    }
    throw std::invalid_argument("unknown tiling");
}

std::string_view tiling_name(Tiling tiling) {
    return tiling_system(tiling).spec().name;
}

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
    case Seed::pentagon_5:
        return "pentagon-5";
    case Seed::pentagon_3:
        return "pentagon-3";
    case Seed::pentagon_2:
        return "pentagon-2";
    case Seed::diamond:
        return "diamond";
    case Seed::boat:
        return "boat";
    case Seed::triangle:
        return "triangle";
    case Seed::square:
        return "square";
    case Seed::rhomb:
        return "rhomb";
    case Seed::octagon:
        return "octagon";
    case Seed::dodecagon:
        return "dodecagon";
    }
    throw std::invalid_argument("unknown seed");
}

bool seed_supported(Tiling tiling, Seed seed) {
    return tiling_system(tiling).find_seed(seed_name(seed)) != nullptr;
}

std::vector<RobinsonTriangle> make_seed(Tiling tiling, Seed seed) {
    if (tiling != Tiling::p2 && tiling != Tiling::p3) {
        throw std::invalid_argument(
            "tiling does not use Robinson-triangle substitution");
    }
    if (!seed_supported(tiling, seed)) {
        throw std::invalid_argument(std::string(seed_name(seed)) +
                                    " seed is not available for " +
                                    std::string(tiling_name(tiling)));
    }
    return tiling == Tiling::p2 ? p2_seed(seed) : p3_seed(seed);
}

std::vector<RobinsonTriangle> subdivide(std::span<const RobinsonTriangle> triangles,
                                        Tiling tiling) {
    if (tiling != Tiling::p2 && tiling != Tiling::p3) {
        throw std::invalid_argument(
            "tiling does not use Robinson-triangle substitution");
    }
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
    if (tiling != Tiling::p2 && tiling != Tiling::p3) {
        throw std::invalid_argument(
            "tiling does not use Robinson-triangle substitution");
    }
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
    if (tiling != Tiling::p2 && tiling != Tiling::p3) {
        throw std::invalid_argument("tiling does not use Robinson-triangle pairs");
    }
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
            const auto tile_shape = shape(first_triangle.kind, tiling);
            Tile tile{
                tile_shape,
                {first_seam.first, negative, first_seam.second, positive},
                static_cast<std::uint8_t>(tile_shape == Shape::kite ||
                                                  tile_shape == Shape::thick_rhomb
                                              ? 0
                                              : maximum_fill),
            };
            canonicalise(tile.vertices);
            tiles.push_back(tile);
        }

        first = last;
    }

    sort_tiles(tiles);

    return tiles;
}

std::vector<Tile> largest_component(std::span<const Tile> tiles) {
    if (tiles.empty()) {
        return {};
    }

    std::vector<IndexedEdge> indexed;
    indexed.reserve(std::accumulate(tiles.begin(), tiles.end(), std::size_t{},
                                    [](std::size_t count, const Tile& tile) {
                                        return count + tile.vertices.size();
                                    }));
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

std::vector<Tile> generate_tiles(Tiling tiling, Seed seed, unsigned depth) {
    return tiling_system(tiling).generate(seed_name(seed), depth);
}

std::size_t predicted_triangle_count(Tiling tiling, Seed seed, unsigned depth) {
    if (tiling != Tiling::p2 && tiling != Tiling::p3) {
        throw std::invalid_argument(
            "tiling does not use Robinson-triangle substitution");
    }
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

namespace detail {

std::vector<TilingSystem> make_penrose_systems() {
    std::vector<TilingSystem> systems;
    systems.reserve(3);
    systems.push_back(make_p1_system());
    systems.push_back(make_robinson_system(Tiling::p2));
    systems.push_back(make_robinson_system(Tiling::p3));
    return systems;
}

} // namespace detail

} // namespace aper
