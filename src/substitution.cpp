#include "penrose.hpp"
#include "system.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstdint>
#include <numbers>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace aper::detail {
namespace {

constexpr double quantisation = 1.0e10;

enum class Kind : std::uint8_t {
    ammann_a,
    ammann_b,
    ammann_rhomb,
    stampfli_triangle,
    stampfli_rhomb,
    stampfli_square,
};

struct Similarity {
    Point translation{};
    Point multiplier{1.0, 0.0};
    bool reflected = false;
};

struct State {
    Kind kind;
    Similarity pose;
};

struct Polygon {
    std::array<Point, 4> vertices{};
    std::size_t size = 0;
};

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
    State state;
};

struct PinwheelTriangle {
    // Small-angle, right-angle, and large-angle vertices, in that order.
    Point p;
    Point q;
    Point r;
};

Point apply(const Similarity& transform, Point point) {
    if (transform.reflected) {
        point = std::conj(point);
    }
    return transform.translation + transform.multiplier * point;
}

Similarity placement(Point translation, double degrees, double scale = 1.0,
                     bool reflected = false) {
    return {
        translation,
        std::polar(scale, degrees * std::numbers::pi / 180.0),
        reflected,
    };
}

State child(Kind kind, double x, double y, double degrees, double scale) {
    return {kind, placement(scale * Point{x, y}, degrees, scale)};
}

Polygon canonical_polygon(Kind kind) {
    constexpr Point origin{};
    constexpr Point one{1.0, 0.0};
    constexpr Point imaginary{0.0, 1.0};
    const auto sqrt_two = std::sqrt(2.0);
    const auto sqrt_three = std::sqrt(3.0);

    switch (kind) {
    case Kind::ammann_a:
    case Kind::ammann_b:
        return {{{origin, one, imaginary, origin}}, 3};
    case Kind::ammann_rhomb: {
        const Point diagonal{1.0 / sqrt_two, 1.0 / sqrt_two};
        return {{{origin, one, one + diagonal, diagonal}}, 4};
    }
    case Kind::stampfli_triangle: {
        const Point diagonal{0.5, sqrt_three / 2.0};
        return {{{origin, one, diagonal, origin}}, 3};
    }
    case Kind::stampfli_rhomb: {
        const Point diagonal{sqrt_three / 2.0, 0.5};
        return {{{origin, one, one + diagonal, diagonal}}, 4};
    }
    case Kind::stampfli_square:
        return {{{origin, one, one + imaginary, imaginary}}, 4};
    }
    throw std::invalid_argument("unknown straight-edged prototile");
}

Polygon transformed_polygon(const State& state) {
    auto polygon = canonical_polygon(state.kind);
    for (std::size_t i = 0; i < polygon.size; ++i) {
        polygon.vertices[i] = apply(state.pose, polygon.vertices[i]);
    }
    return polygon;
}

QuantisedPoint quantise(Point point) {
    return {
        std::llround(point.real() * quantisation),
        std::llround(point.imag() * quantisation),
    };
}

EdgeKey edge_key(Point first, Point second) {
    auto a = quantise(first);
    auto b = quantise(second);
    if (b < a) {
        std::swap(a, b);
    }
    return {a, b};
}

double cross(Point first, Point second) {
    return first.real() * second.imag() - first.imag() * second.real();
}

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
        return std::tie(lhs_centre.y, lhs_centre.x, lhs.shape, lhs.fill) <
               std::tie(rhs_centre.y, rhs_centre.x, rhs.shape, rhs.fill);
    });
}

std::span<const State> ammann_rule(Kind kind) {
    const auto sqrt_two = std::sqrt(2.0);
    const auto d = 1.0 / sqrt_two;
    const auto inflation = 1.0 + sqrt_two;
    const auto scale = 1.0 / inflation;

    static const auto a = std::to_array<State>({
        child(Kind::ammann_a, d, 1.0 + d, 135.0, scale),
        child(Kind::ammann_a, d, d, -135.0, scale),
        child(Kind::ammann_b, d, d, 0.0, scale),
        child(Kind::ammann_rhomb, 0.0, 0.0, 45.0, scale),
        child(Kind::ammann_rhomb, d, d, -45.0, scale),
    });
    static const auto b = std::to_array<State>({
        child(Kind::ammann_b, 1.0 + d, d, -135.0, scale),
        child(Kind::ammann_b, d, d, 135.0, scale),
        child(Kind::ammann_a, d, d, 0.0, scale),
        child(Kind::ammann_rhomb, 0.0, 0.0, 0.0, scale),
        child(Kind::ammann_rhomb, d, d, 90.0, scale),
    });
    static const auto rhomb = std::to_array<State>({
        child(Kind::ammann_rhomb, 0.0, 0.0, 0.0, scale),
        child(Kind::ammann_rhomb, inflation, 0.0, 90.0, scale),
        child(Kind::ammann_rhomb, inflation, 1.0, 0.0, scale),
        child(Kind::ammann_b, 1.0 + d, d, -135.0, scale),
        child(Kind::ammann_b, inflation, 1.0, 45.0, scale),
        child(Kind::ammann_a, 1.0 + d, d, 90.0, scale),
        child(Kind::ammann_a, inflation, 1.0, -90.0, scale),
    });

    switch (kind) {
    case Kind::ammann_a:
        return a;
    case Kind::ammann_b:
        return b;
    case Kind::ammann_rhomb:
        return rhomb;
    case Kind::stampfli_triangle:
    case Kind::stampfli_rhomb:
    case Kind::stampfli_square:
        break;
    }
    throw std::invalid_argument("non-Ammann tile in Ammann-Beenker rule");
}

std::span<const State> stampfli_rule(Kind kind) {
    const auto root_three = std::sqrt(3.0);
    const auto scale = 2.0 - root_three;
    const auto half_root = root_three / 2.0;
    const auto inflation = 2.0 + root_three;
    const auto half_inflation = inflation / 2.0;
    const auto centre = (1.0 + root_three) / 2.0;
    const auto one_plus_root = 1.0 + root_three;
    const auto three_plus_root = (3.0 + root_three) / 2.0;
    const auto high = 1.5 + root_three;
    const auto far = 2.0 + 1.5 * root_three;
    const auto farther = 2.5 + 1.5 * root_three;
    const auto farthest = 3.0 + 1.5 * root_three;

    // These are the unique placements in the Encyclopedia's vector rule.
    // Its triangle group contains one exact duplicate use, omitted here.
    static const auto triangle = std::to_array<State>({
        child(Kind::stampfli_rhomb, 0.0, 0.0, 0.0, scale),
        child(Kind::stampfli_rhomb, 0.0, 0.0, 30.0, scale),
        child(Kind::stampfli_rhomb, inflation, 0.0, 150.0, scale),
        child(Kind::stampfli_rhomb, inflation, 0.0, 120.0, scale),
        child(Kind::stampfli_rhomb, half_inflation, high, 270.0, scale),
        child(Kind::stampfli_rhomb, half_inflation, high, 240.0, scale),
        child(Kind::stampfli_triangle, half_root, 0.5, 0.0, scale),
        child(Kind::stampfli_triangle, half_inflation, 0.5, 0.0, scale),
        child(Kind::stampfli_triangle, centre, centre, 0.0, scale),
        child(Kind::stampfli_triangle, half_inflation, 0.5, 60.0, scale),
        child(Kind::stampfli_triangle, 1.0, 0.0, 330.0, scale),
        child(Kind::stampfli_triangle, one_plus_root, 0.0, 150.0, scale),
        child(Kind::stampfli_triangle, 0.5, half_root, 30.0, scale),
        child(Kind::stampfli_triangle, centre, three_plus_root, 210.0, scale),
        child(Kind::stampfli_triangle, three_plus_root, centre, 30.0, scale),
        child(Kind::stampfli_triangle, three_plus_root, centre, 330.0, scale),
    });
    static const auto rhomb = std::to_array<State>({
        child(Kind::stampfli_rhomb, 0.0, 0.0, 0.0, scale),
        child(Kind::stampfli_triangle, half_root, 0.5, 0.0, scale),
        child(Kind::stampfli_triangle, half_inflation, 0.5, 60.0, scale),
        child(Kind::stampfli_triangle, one_plus_root, 0.0, 150.0, scale),
        child(Kind::stampfli_triangle, 1.0, 0.0, 330.0, scale),
        child(Kind::stampfli_square, half_inflation, 0.5, 330.0, scale),
        child(Kind::stampfli_triangle, one_plus_root, 0.0, 0.0, scale),
        child(Kind::stampfli_triangle, three_plus_root, centre, 330.0, scale),
        child(Kind::stampfli_rhomb, inflation, 0.0, 90.0, scale),
        child(Kind::stampfli_triangle, inflation, 0.0, 30.0, scale),
        child(Kind::stampfli_triangle, high, half_inflation, 300.0, scale),
        child(Kind::stampfli_square, inflation, 1.0, 330.0, scale),
        child(Kind::stampfli_triangle, far, 0.5, 0.0, scale),
        child(Kind::stampfli_triangle, farthest, 0.5, 60.0, scale),
        child(Kind::stampfli_rhomb, farther, centre, 0.0, scale),
        child(Kind::stampfli_triangle, farther, centre, 30.0, scale),
        child(Kind::stampfli_triangle, farther, centre, 90.0, scale),
    });
    static const auto square = std::to_array<State>({
        child(Kind::stampfli_rhomb, 0.0, 0.0, 0.0, scale),
        child(Kind::stampfli_rhomb, 0.0, 0.0, 30.0, scale),
        child(Kind::stampfli_rhomb, 0.0, 0.0, 60.0, scale),
        child(Kind::stampfli_rhomb, inflation, 0.0, 150.0, scale),
        child(Kind::stampfli_rhomb, inflation, 0.0, 120.0, scale),
        child(Kind::stampfli_rhomb, inflation, 0.0, 90.0, scale),
        child(Kind::stampfli_rhomb, inflation, inflation, 240.0, scale),
        child(Kind::stampfli_rhomb, inflation, inflation, 210.0, scale),
        child(Kind::stampfli_rhomb, inflation, inflation, 180.0, scale),
        child(Kind::stampfli_rhomb, 0.0, inflation, 330.0, scale),
        child(Kind::stampfli_rhomb, 0.0, inflation, 300.0, scale),
        child(Kind::stampfli_rhomb, 0.0, inflation, 270.0, scale),
        child(Kind::stampfli_triangle, 1.0, 0.0, 330.0, scale),
        child(Kind::stampfli_triangle, one_plus_root, 0.0, 150.0, scale),
        child(Kind::stampfli_triangle, 1.0, inflation, 330.0, scale),
        child(Kind::stampfli_triangle, one_plus_root, inflation, 150.0, scale),
        child(Kind::stampfli_triangle, 0.0, 1.0, 60.0, scale),
        child(Kind::stampfli_triangle, 0.0, one_plus_root, 240.0, scale),
        child(Kind::stampfli_triangle, inflation, 1.0, 60.0, scale),
        child(Kind::stampfli_triangle, inflation, one_plus_root, 240.0, scale),
        child(Kind::stampfli_triangle, half_root, 0.5, 0.0, scale),
        child(Kind::stampfli_triangle, half_inflation, 0.5, 0.0, scale),
        child(Kind::stampfli_triangle, half_inflation, 0.5, 60.0, scale),
        child(Kind::stampfli_triangle, 0.5, half_inflation, 30.0, scale),
        child(Kind::stampfli_triangle, 0.5, half_inflation, 330.0, scale),
        child(Kind::stampfli_triangle, 0.5, half_inflation, 270.0, scale),
        child(Kind::stampfli_triangle, half_inflation, high, 180.0, scale),
        child(Kind::stampfli_triangle, half_inflation, high, 240.0, scale),
        child(Kind::stampfli_triangle, half_inflation, high, 300.0, scale),
        child(Kind::stampfli_triangle, high, half_inflation, 90.0, scale),
        child(Kind::stampfli_triangle, high, half_inflation, 150.0, scale),
        child(Kind::stampfli_triangle, high, half_inflation, 210.0, scale),
        child(Kind::stampfli_square, centre, centre, 0.0, scale),
    });

    switch (kind) {
    case Kind::stampfli_triangle:
        return triangle;
    case Kind::stampfli_rhomb:
        return rhomb;
    case Kind::stampfli_square:
        return square;
    case Kind::ammann_a:
    case Kind::ammann_b:
    case Kind::ammann_rhomb:
        break;
    }
    throw std::invalid_argument("non-Stampfli tile in Stampfli rule");
}

std::vector<State> ammann_seed(Seed seed) {
    if (seed == Seed::square) {
        return {
            {Kind::ammann_a, {}},
            {Kind::ammann_b, placement({1.0, 1.0}, 180.0)},
        };
    }
    if (seed == Seed::rhomb) {
        return {{Kind::ammann_rhomb, {}}};
    }
    if (seed == Seed::octagon) {
        std::vector<State> states;
        states.reserve(8);
        for (int turn = 0; turn < 8; ++turn) {
            states.push_back({Kind::ammann_rhomb, placement({}, 45.0 * turn)});
        }
        return states;
    }
    throw std::invalid_argument("seed is not available for Ammann-Beenker");
}

std::vector<Tile> ammann_tiles(std::span<const State> states) {
    std::vector<Tile> tiles;
    std::vector<IndexedTriangle> triangles;
    for (const auto& state : states) {
        const auto polygon = transformed_polygon(state);
        if (state.kind == Kind::ammann_rhomb) {
            std::vector<Point> vertices(polygon.vertices.begin(),
                                        polygon.vertices.begin() + 4);
            canonicalise(vertices);
            tiles.push_back({Shape::ammann_rhomb, std::move(vertices), maximum_fill});
            continue;
        }
        triangles.push_back(
            {edge_key(polygon.vertices[1], polygon.vertices[2]), state});
    }

    std::sort(triangles.begin(), triangles.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.seam < rhs.seam; });
    for (std::size_t first = 0; first < triangles.size();) {
        auto last = first + 1;
        while (last < triangles.size() &&
               triangles[last].seam == triangles[first].seam) {
            ++last;
        }
        if (last - first > 2) {
            throw std::runtime_error("more than two Ammann triangles share a seam");
        }
        if (last - first == 2) {
            const auto& first_state = triangles[first].state;
            const auto& second_state = triangles[first + 1].state;
            if (first_state.kind == second_state.kind) {
                throw std::runtime_error("like Ammann triangles share a square seam");
            }
            const auto a = transformed_polygon(first_state);
            const auto b = transformed_polygon(second_state);
            const auto edge = a.vertices[2] - a.vertices[1];
            const auto side_a = cross(edge, a.vertices[0] - a.vertices[1]);
            const auto side_b = cross(edge, b.vertices[0] - a.vertices[1]);
            if (side_a * side_b >= 0.0) {
                throw std::runtime_error("paired Ammann triangles lie on one side");
            }
            const auto negative = side_a < 0.0 ? a.vertices[0] : b.vertices[0];
            const auto positive = side_a > 0.0 ? a.vertices[0] : b.vertices[0];
            std::vector<Point> vertices{a.vertices[1], negative, a.vertices[2],
                                        positive};
            canonicalise(vertices);
            tiles.push_back({Shape::square, std::move(vertices), 0});
        }
        first = last;
    }
    sort_tiles(tiles);
    return tiles;
}

std::vector<PinwheelTriangle>
subdivide_pinwheel(std::span<const PinwheelTriangle> triangles) {
    std::vector<PinwheelTriangle> children;
    children.reserve(5 * triangles.size());
    for (const auto& triangle : triangles) {
        const auto u = triangle.p + 2.0 * (triangle.r - triangle.p) / 5.0;
        const auto v = (triangle.p + triangle.q) / 2.0;
        const auto w = triangle.p + 4.0 * (triangle.r - triangle.p) / 5.0;
        const auto x = (triangle.q + w) / 2.0;
        children.push_back({triangle.p, u, v});
        children.push_back({w, u, v});
        children.push_back({v, x, w});
        children.push_back({v, x, triangle.q});
        children.push_back({triangle.q, w, triangle.r});
    }
    return children;
}

std::uint8_t pinwheel_fill(const PinwheelTriangle& triangle) {
    const auto theta = std::arg(triangle.q - triangle.p);
    const auto blend = (1.0 - std::cos(4.0 * theta)) / 2.0;
    return static_cast<std::uint8_t>(
        std::llround(static_cast<double>(maximum_fill) * blend));
}

std::vector<State> stampfli_seed(Seed seed) {
    if (seed == Seed::triangle) {
        return {{Kind::stampfli_triangle, {}}};
    }
    if (seed == Seed::square) {
        return {{Kind::stampfli_square, {}}};
    }
    if (seed == Seed::rhomb) {
        return {{Kind::stampfli_rhomb, {}}};
    }
    if (seed == Seed::dodecagon) {
        std::vector<State> states;
        states.reserve(12);
        for (int turn = 0; turn < 12; ++turn) {
            states.push_back({Kind::stampfli_rhomb, placement({}, 30.0 * turn)});
        }
        return states;
    }
    throw std::invalid_argument("seed is not available for Stampfli 12-fold 1");
}

std::vector<Point> polygon_vertices(Kind kind) {
    const auto polygon = canonical_polygon(kind);
    return {polygon.vertices.begin(),
            polygon.vertices.begin() + static_cast<std::ptrdiff_t>(polygon.size)};
}

aper::Similarity public_similarity(const Similarity& similarity) {
    return {similarity.translation, similarity.multiplier, similarity.reflected};
}

Similarity private_similarity(const aper::Similarity& similarity) {
    return {similarity.translation(), similarity.multiplier(), similarity.reflected()};
}

PrototileId ammann_id(Kind kind) {
    switch (kind) {
    case Kind::ammann_a:
        return 0;
    case Kind::ammann_b:
        return 1;
    case Kind::ammann_rhomb:
        return 2;
    case Kind::stampfli_triangle:
    case Kind::stampfli_rhomb:
    case Kind::stampfli_square:
        break;
    }
    throw std::invalid_argument("kind is not an Ammann-Beenker prototile");
}

Kind ammann_kind(PrototileId id) {
    switch (id) {
    case 0:
        return Kind::ammann_a;
    case 1:
        return Kind::ammann_b;
    case 2:
        return Kind::ammann_rhomb;
    default:
        throw std::invalid_argument("unknown Ammann-Beenker prototile");
    }
}

PrototileId stampfli_id(Kind kind) {
    switch (kind) {
    case Kind::stampfli_triangle:
        return 0;
    case Kind::stampfli_rhomb:
        return 1;
    case Kind::stampfli_square:
        return 2;
    case Kind::ammann_a:
    case Kind::ammann_b:
    case Kind::ammann_rhomb:
        break;
    }
    throw std::invalid_argument("kind is not a Stampfli prototile");
}

Patch state_patch(std::span<const State> states, PrototileId (*id)(Kind)) {
    std::vector<Placement> placements;
    placements.reserve(states.size());
    for (const auto& state : states) {
        placements.push_back({id(state.kind), public_similarity(state.pose)});
    }
    return Patch{std::move(placements)};
}

class AmmannProjector final : public PatchProjector {
  public:
    std::vector<Tile> project(const TilingSystem&, const Patch& patch) const override {
        std::vector<State> states;
        states.reserve(patch.size());
        for (const auto& placement : patch.placements()) {
            states.push_back(
                {ammann_kind(placement.prototile), private_similarity(placement.pose)});
        }
        return ammann_tiles(states);
    }
};

Placement pinwheel_placement(const Prototile& prototile,
                             const PinwheelTriangle& triangle) {
    const std::array target{triangle.p, triangle.q, triangle.r};
    for (const auto reflected : {false, true}) {
        std::array<Point, 3> source{};
        for (std::size_t i = 0; i < source.size(); ++i) {
            source[i] =
                reflected ? std::conj(prototile.boundary[i]) : prototile.boundary[i];
        }
        const auto multiplier = (target[1] - target[0]) / (source[1] - source[0]);
        const auto translation = target[0] - multiplier * source[0];
        if (std::abs(translation + multiplier * source[2] - target[2]) < 1.0e-8) {
            return {prototile.id, aper::Similarity{translation, multiplier, reflected}};
        }
    }
    throw std::runtime_error("triangle is not similar to the pinwheel prototile");
}

class PinwheelProjector final : public PatchProjector {
  public:
    std::vector<Tile> project(const TilingSystem& system,
                              const Patch& patch) const override {
        std::vector<Tile> tiles;
        tiles.reserve(patch.size());
        for (const auto& placement : patch.placements()) {
            const auto& prototile = system.prototile(placement.prototile);
            if (prototile.id != 0 || prototile.boundary.size() != 3) {
                throw std::runtime_error(
                    "non-pinwheel prototile in pinwheel projection");
            }
            PinwheelTriangle triangle{
                placement.pose.apply(prototile.boundary[0]),
                placement.pose.apply(prototile.boundary[1]),
                placement.pose.apply(prototile.boundary[2]),
            };
            std::vector<Point> vertices{triangle.p, triangle.q, triangle.r};
            canonicalise(vertices);
            tiles.push_back({Shape::pinwheel_triangle, std::move(vertices),
                             pinwheel_fill(triangle)});
        }
        sort_tiles(tiles);
        return tiles;
    }
};

TilingSystem make_ammann_system() {
    std::vector<Prototile> prototiles{
        {0, "triangle-a", Shape::ammann_triangle_a, polygon_vertices(Kind::ammann_a),
         0},
        {1, "triangle-b", Shape::ammann_triangle_b, polygon_vertices(Kind::ammann_b),
         maximum_fill},
        {2, "rhomb", Shape::ammann_rhomb, polygon_vertices(Kind::ammann_rhomb),
         static_cast<std::uint8_t>((maximum_fill + 1) / 2)},
    };
    std::vector<RuleEntry> rules;
    for (const auto kind : {Kind::ammann_a, Kind::ammann_b, Kind::ammann_rhomb}) {
        rules.push_back({ammann_id(kind), state_patch(ammann_rule(kind), ammann_id)});
    }
    std::vector<SeedPatch> seeds{
        {"octagon", state_patch(ammann_seed(Seed::octagon), ammann_id), 1},
        {"square", state_patch(ammann_seed(Seed::square), ammann_id), 1},
        {"rhomb", state_patch(ammann_seed(Seed::rhomb), ammann_id), 1},
    };
    return {
        {"ammann-beenker",
         "Ammann-Beenker",
         {"ab"},
         "octagon",
         {minimum_depth, default_ammann_beenker_depth, maximum_ammann_beenker_depth}},
        std::move(prototiles),
        SubstitutionRule{1.0 + std::sqrt(2.0), std::move(rules)},
        std::move(seeds),
        std::make_shared<const AmmannProjector>(),
    };
}

TilingSystem make_pinwheel_system() {
    const PinwheelTriangle parent{{0.0, 0.0}, {2.0, 0.0}, {2.0, 1.0}};
    Prototile prototile{
        0, "triangle", Shape::pinwheel_triangle, {parent.p, parent.q, parent.r}, 0,
    };
    const std::array parents{parent};
    std::vector<Placement> children;
    for (const auto& child : subdivide_pinwheel(parents)) {
        children.push_back(pinwheel_placement(prototile, child));
    }
    return {
        {"pinwheel",
         "Pinwheel",
         {},
         "triangle",
         {minimum_depth, default_pinwheel_depth, maximum_pinwheel_depth}},
        {prototile},
        SubstitutionRule{std::sqrt(5.0), {{0, Patch{std::move(children)}}}},
        {{"triangle", Patch{{Placement{0, {}}}}, 1}},
        std::make_shared<const PinwheelProjector>(),
    };
}

TilingSystem make_stampfli_system() {
    std::vector<Prototile> prototiles{
        {0, "triangle", Shape::equilateral_triangle,
         polygon_vertices(Kind::stampfli_triangle), maximum_fill},
        {1, "rhomb", Shape::stampfli_rhomb, polygon_vertices(Kind::stampfli_rhomb),
         static_cast<std::uint8_t>((maximum_fill + 1) / 2)},
        {2, "square", Shape::square, polygon_vertices(Kind::stampfli_square), 0},
    };
    std::vector<RuleEntry> rules;
    for (const auto kind :
         {Kind::stampfli_triangle, Kind::stampfli_rhomb, Kind::stampfli_square}) {
        rules.push_back(
            {stampfli_id(kind), state_patch(stampfli_rule(kind), stampfli_id)});
    }
    std::vector<SeedPatch> seeds{
        {"dodecagon", state_patch(stampfli_seed(Seed::dodecagon), stampfli_id), 1},
        {"triangle", state_patch(stampfli_seed(Seed::triangle), stampfli_id), 1},
        {"square", state_patch(stampfli_seed(Seed::square), stampfli_id), 1},
        {"rhomb", state_patch(stampfli_seed(Seed::rhomb), stampfli_id), 1},
    };
    return {
        {"stampfli",
         "Stampfli 12-fold 1",
         {"stampfli-12-fold-1"},
         "dodecagon",
         {minimum_depth, default_stampfli_depth, maximum_stampfli_depth}},
        std::move(prototiles),
        SubstitutionRule{2.0 + std::sqrt(3.0), std::move(rules), true},
        std::move(seeds),
        identity_projector(),
    };
}

} // namespace

std::vector<TilingSystem> make_straight_systems() {
    std::vector<TilingSystem> systems;
    systems.reserve(3);
    systems.push_back(make_ammann_system());
    systems.push_back(make_pinwheel_system());
    systems.push_back(make_stampfli_system());
    return systems;
}

} // namespace aper::detail
