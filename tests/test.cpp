#include "bank.hpp"
#include "discovery.hpp"
#include "pdf.hpp"
#include "penrose.hpp"
#include "system.hpp"
#include "version.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <numbers>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

static_assert(!std::is_constructible_v<aper::PatchView, aper::TilingSystem&&,
                                       std::string_view, unsigned>);
static_assert(!std::is_constructible_v<aper::RuleView, aper::TilingSystem&&>);

int failures = 0;

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            std::cerr << __FILE__ << ':' << __LINE__                                   \
                      << ": check failed: " #condition "\n";                           \
            ++failures;                                                                \
        }                                                                              \
    } while (false)

bool close(double lhs, double rhs, double tolerance = 1.0e-9) {
    return std::abs(lhs - rhs) <= tolerance;
}

double cross(aper::Point a, aper::Point b) {
    return a.real() * b.imag() - a.imag() * b.real();
}

double area(const aper::RobinsonTriangle& triangle) {
    return std::abs(cross(triangle.base_a - triangle.apex,
                          triangle.base_b - triangle.apex)) /
           2.0;
}

double signed_area(const aper::Tile& tile) {
    double twice_area = 0.0;
    for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
        twice_area +=
            cross(tile.vertices[i], tile.vertices[(i + 1) % tile.vertices.size()]);
    }
    return twice_area / 2.0;
}

double turn(aper::Point a, aper::Point b, aper::Point c) { return cross(b - a, c - b); }

std::size_t occurrences(const std::string& text, std::string_view needle) {
    std::size_t count = 0;
    for (auto position = text.find(needle); position != std::string::npos;
         position = text.find(needle, position + needle.size())) {
        ++count;
    }
    return count;
}

struct SeedFixture {
    aper::Tiling tiling;
    aper::Seed seed;
    std::size_t acute;
    std::size_t obtuse;
    std::size_t initial_tiles;
    std::size_t next_tiles;
};

struct P1Fixture {
    aper::Seed seed;
    aper::Shape shape;
    std::size_t children;
    std::size_t shared_edges;
    std::array<std::size_t, 6> kinds;
};

struct TileTopology {
    std::size_t vertices;
    std::size_t edges;
    std::size_t boundary_edges;
    std::size_t shared_edges;
};

std::vector<aper::Placement> square_quarters() {
    return {
        {0, aper::Similarity{{0.0, 0.0}, {0.5, 0.0}}},
        {0, aper::Similarity{{0.5, 0.0}, {0.5, 0.0}}},
        {0, aper::Similarity{{0.0, 0.5}, {0.5, 0.0}}},
        {0, aper::Similarity{{0.5, 0.5}, {0.5, 0.0}}},
    };
}

aper::TilingSystem make_square_system(std::string id,
                                      std::vector<aper::Placement> placements,
                                      std::vector<aper::SourceReference> sources = {}) {
    return {
        {std::move(id), "Grid", {}, "square", {1, 2, 7}, std::move(sources)},
        {{0,
          "square",
          aper::Shape::generic_polygon,
          {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
          0}},
        aper::SubstitutionRule{2.0, {{0, aper::Patch{std::move(placements)}}}},
        {{"square", aper::Patch{{aper::Placement{0, {}}}}, 1}},
        aper::identity_projector(),
    };
}

aper::TilingSystem make_reframed_square_system(bool reflected) {
    const aper::Point origin{3.0, -2.0};
    const aper::Point axis{0.0, 2.0};
    const auto frame = [=](aper::Point point) {
        return origin + axis * (reflected ? std::conj(point) : point);
    };
    std::vector<aper::Point> boundary;
    for (const auto point : std::array{aper::Point{0.0, 0.0}, aper::Point{1.0, 0.0},
                                       aper::Point{1.0, 1.0}, aper::Point{0.0, 1.0}}) {
        boundary.push_back(frame(point));
    }
    std::vector<aper::Placement> placements;
    for (const auto& placement : square_quarters()) {
        const auto translation = placement.pose.translation();
        const auto framed_translation =
            origin + axis * (reflected ? std::conj(translation) : translation) -
            0.5 * origin;
        placements.push_back({0, aper::Similarity{framed_translation, {0.5, 0.0}}});
    }
    return {
        {reflected ? "reflected-grid" : "reframed-grid",
         "Reframed grid",
         {},
         "square",
         {1, 2, 7}},
        {{0, "square", aper::Shape::generic_polygon, std::move(boundary), 0}},
        aper::SubstitutionRule{2.0, {{0, aper::Patch{std::move(placements)}}}},
        {{"square", aper::Patch{{aper::Placement{0, {}}}}, 1}},
        aper::identity_projector(),
    };
}

aper::TilingSystem make_typed_square_system(bool relabelled, bool reflected = false) {
    const auto renamed = [relabelled](aper::PrototileId id) {
        return relabelled ? 1 - id : id;
    };
    const std::array patterns{
        std::array<aper::PrototileId, 4>{0, 0, 0, 1},
        std::array<aper::PrototileId, 4>{1, 0, 1, 1},
    };
    const std::array translations{
        aper::Point{0.0, 0.0},
        aper::Point{0.5, 0.0},
        aper::Point{0.0, 0.5},
        aper::Point{0.5, 0.5},
    };
    std::vector<aper::RuleEntry> rules;
    for (aper::PrototileId parent = 0; parent < patterns.size(); ++parent) {
        std::vector<aper::Placement> placements;
        for (std::size_t i = 0; i < translations.size(); ++i) {
            const auto translation =
                reflected ? std::conj(translations[i]) : translations[i];
            placements.push_back({renamed(patterns[parent][i]),
                                  aper::Similarity{translation, {0.5, 0.0}}});
        }
        rules.push_back({renamed(parent), aper::Patch{std::move(placements)}});
    }
    if (relabelled) {
        std::reverse(rules.begin(), rules.end());
    }
    std::vector<aper::Point> square{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    if (reflected) {
        std::transform(square.begin(), square.end(), square.begin(),
                       [](aper::Point point) { return std::conj(point); });
    }
    return {
        {relabelled ? "typed-swapped" : "typed", "Typed grid", {}, "seed", {1, 2, 5}},
        {{0, "first", aper::Shape::generic_polygon, square, 0},
         {1, "second", aper::Shape::generic_polygon, square, aper::maximum_fill}},
        aper::SubstitutionRule{2.0, std::move(rules)},
        {{"seed", aper::Patch{{aper::Placement{renamed(0), {}}}}, 1}},
        aper::identity_projector(),
    };
}

aper::TilingSystem make_triangular_binary_system() {
    auto first = square_quarters();
    auto second = square_quarters();
    constexpr std::array<aper::PrototileId, 4> first_pattern{0, 1, 1, 0};
    constexpr std::array<aper::PrototileId, 4> second_pattern{1, 0, 0, 1};
    for (std::size_t i = 0; i < first.size(); ++i) {
        first[i].prototile = first_pattern[i];
        second[i].prototile = second_pattern[i];
    }
    const std::vector<aper::Point> triangle{{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};
    return {
        {"triangular-binary", "Triangular binary", {}, "a", {1, 4, 7}},
        {{0, "a", aper::Shape::generic_polygon, triangle, 0},
         {1, "b", aper::Shape::generic_polygon, triangle, aper::maximum_fill}},
        aper::SubstitutionRule{
            2.0,
            {{0, aper::Patch{std::move(first)}}, {1, aper::Patch{std::move(second)}}}},
        {{"a", aper::Patch{{aper::Placement{0, {}}}}, 1},
         {"b", aper::Patch{{aper::Placement{1, {}}}}, 1}},
        aper::identity_projector(),
    };
}

class DuplicateSquareSource final : public aper::CandidateSource {
  public:
    void enumerate(const Visitor& visit) const override {
        auto reversed = square_quarters();
        std::reverse(reversed.begin(), reversed.end());
        if (!visit(make_square_system("first-square", square_quarters()))) {
            return;
        }
        (void)visit(make_square_system("second-square", std::move(reversed)));
    }
};

std::vector<aper::TilingSystem> collect(const aper::CandidateSource& source) {
    std::vector<aper::TilingSystem> result;
    source.enumerate([&](aper::TilingSystem candidate) {
        result.push_back(std::move(candidate));
        return true;
    });
    return result;
}

TileTopology topology(std::span<const aper::Tile> tiles) {
    using QuantisedPoint = std::array<long long, 2>;
    using Edge = std::array<QuantisedPoint, 2>;
    const auto quantise = [](aper::Point point) {
        return QuantisedPoint{std::llround(point.real() * 1.0e9),
                              std::llround(point.imag() * 1.0e9)};
    };

    std::vector<QuantisedPoint> vertices;
    std::vector<Edge> edges;
    for (const auto& tile : tiles) {
        for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
            auto first = quantise(tile.vertices[i]);
            auto second = quantise(tile.vertices[(i + 1) % tile.vertices.size()]);
            vertices.push_back(first);
            if (second < first) {
                std::swap(first, second);
            }
            edges.push_back({first, second});
        }
    }
    std::sort(vertices.begin(), vertices.end());
    std::sort(edges.begin(), edges.end());

    const auto vertex_count = static_cast<std::size_t>(
        std::distance(vertices.begin(), std::unique(vertices.begin(), vertices.end())));
    std::size_t edge_count = 0;
    std::size_t boundary = 0;
    std::size_t shared = 0;
    for (std::size_t first = 0; first < edges.size();) {
        auto last = first + 1;
        while (last < edges.size() && edges[last] == edges[first]) {
            ++last;
        }
        CHECK(last - first <= 2);
        ++edge_count;
        boundary += static_cast<std::size_t>(last - first == 1);
        shared += static_cast<std::size_t>(last - first == 2);
        first = last;
    }
    return {vertex_count, edge_count, boundary, shared};
}

void test_p1_substitution() {
    using enum aper::Shape;
    constexpr std::array fixtures{
        P1Fixture{aper::Seed::pentagon_5, pentagon_5, 6, 5, {0, 0, 0, 1, 5, 0}},
        P1Fixture{aper::Seed::pentagon_3, pentagon_3, 7, 7, {0, 0, 1, 1, 3, 2}},
        P1Fixture{aper::Seed::pentagon_2, pentagon_2, 8, 9, {0, 0, 2, 1, 1, 4}},
        P1Fixture{aper::Seed::diamond, diamond, 3, 3, {1, 1, 0, 0, 0, 1}},
        P1Fixture{aper::Seed::boat, boat, 7, 9, {1, 3, 0, 0, 0, 3}},
        P1Fixture{aper::Seed::star, star, 11, 15, {1, 5, 0, 0, 0, 5}},
    };

    const auto kind_index = [](aper::Shape shape) {
        switch (shape) {
        case aper::Shape::star:
            return std::size_t{0};
        case aper::Shape::boat:
            return std::size_t{1};
        case aper::Shape::diamond:
            return std::size_t{2};
        case aper::Shape::pentagon_5:
            return std::size_t{3};
        case aper::Shape::pentagon_3:
            return std::size_t{4};
        case aper::Shape::pentagon_2:
            return std::size_t{5};
        case aper::Shape::generic_polygon:
        case aper::Shape::kite:
        case aper::Shape::dart:
        case aper::Shape::thin_rhomb:
        case aper::Shape::thick_rhomb:
        case aper::Shape::square:
        case aper::Shape::ammann_rhomb:
        case aper::Shape::robinson_acute:
        case aper::Shape::robinson_obtuse:
        case aper::Shape::ammann_triangle_a:
        case aper::Shape::ammann_triangle_b:
        case aper::Shape::pinwheel_triangle:
        case aper::Shape::equilateral_triangle:
        case aper::Shape::stampfli_rhomb:
            break;
        }
        return std::size_t{6};
    };

    for (const auto& fixture : fixtures) {
        const auto seed = aper::generate_tiles(aper::Tiling::p1, fixture.seed, 0);
        CHECK(seed.size() == 1);
        CHECK(seed.front().shape == fixture.shape);

        const auto children = aper::generate_tiles(aper::Tiling::p1, fixture.seed, 1);
        CHECK(children.size() == fixture.children);
        CHECK(aper::largest_component(children).size() == children.size());
        const auto child_topology = topology(children);
        CHECK(child_topology.shared_edges == fixture.shared_edges);
        CHECK(child_topology.vertices + children.size() == child_topology.edges + 1);

        std::array<std::size_t, 6> kinds{};
        for (const auto& tile : children) {
            const auto index = kind_index(tile.shape);
            CHECK(index < kinds.size());
            if (index < kinds.size()) {
                ++kinds[index];
            }
        }
        CHECK(kinds == fixture.kinds);

        constexpr unsigned topology_depth = 3;
        const auto deeper =
            aper::generate_tiles(aper::Tiling::p1, fixture.seed, topology_depth);
        CHECK(aper::largest_component(deeper).size() == deeper.size());
        const auto deeper_topology = topology(deeper);
        CHECK(deeper_topology.vertices + deeper.size() == deeper_topology.edges + 1);
        auto expected_boundary = seed.front().vertices.size();
        for (unsigned generation = 0; generation < topology_depth; ++generation) {
            expected_boundary *= 4;
        }
        CHECK(deeper_topology.boundary_edges == expected_boundary);
    }

    constexpr std::array<std::size_t, 6> pentagon_counts{1, 6, 41, 271, 1806, 12161};
    for (unsigned depth = 0; depth < pentagon_counts.size(); ++depth) {
        const auto tiles =
            aper::generate_tiles(aper::Tiling::p1, aper::Seed::pentagon_5, depth);
        CHECK(tiles.size() == pentagon_counts[depth]);
        CHECK(aper::largest_component(tiles).size() == tiles.size());
    }
}

void test_substitution_systems() {
    const auto& catalogue = aper::tiling_catalogue();
    CHECK(catalogue.systems().size() == 7);
    constexpr std::array<std::string_view, 7> source_records{
        "penrose-pentagon-boat-star",
        "penrose-kite-dart",
        "penrose-rhomb",
        "ammann-beenker-rhomb-triangle",
        "pinwheel",
        "stampflis-12-fold-1",
        "thue-morse-2d",
    };
    for (std::size_t i = 0;
         i < std::min(catalogue.systems().size(), source_records.size()); ++i) {
        const auto& sources = catalogue.systems()[i].spec().sources;
        CHECK(sources.size() == 1);
        if (sources.empty()) {
            continue;
        }
        CHECK(sources.front().collection == "Tilings Encyclopedia");
        CHECK(sources.front().record == source_records[i]);
        CHECK(sources.front().url ==
              "https://tilings.math.uni-bielefeld.de/substitution/" +
                  std::string(source_records[i]) + '/');
        CHECK(sources.front().citation.find("Tilings Encyclopedia") !=
              std::string::npos);
        CHECK(sources.front().licence_url ==
              "https://creativecommons.org/licenses/by-nc-sa/2.0/");
    }
    CHECK(catalogue.find("p1") == &aper::tiling_system(aper::Tiling::p1));
    CHECK(catalogue.find("pentagon-boat-star") ==
          &aper::tiling_system(aper::Tiling::p1));
    CHECK(catalogue.find("kite-dart") == &aper::tiling_system(aper::Tiling::p2));
    CHECK(catalogue.find("rhomb") == &aper::tiling_system(aper::Tiling::p3));
    CHECK(catalogue.find("ab") == &aper::tiling_system(aper::Tiling::ammann_beenker));
    CHECK(catalogue.find("stampfli-12-fold-1") ==
          &aper::tiling_system(aper::Tiling::stampfli));
    CHECK(catalogue.find("thue-morse-2d") == &catalogue.systems().back());
    CHECK(catalogue.find("thue-morse") == &catalogue.systems().back());
    CHECK(catalogue.find("unknown") == nullptr);

    for (const auto& system : catalogue.systems()) {
        CHECK(system.validate().empty());
        CHECK(!system.prototiles().empty());
        CHECK(system.rule().entries().size() == system.prototiles().size());
        CHECK(system.rule().inflation() > 1.0);
        CHECK(system.find_seed(system.spec().default_seed) != nullptr);

        const auto matrix = system.rule().incidence_matrix(system.prototiles().size());
        CHECK(matrix.size() == system.prototiles().size());
        for (const auto& row : matrix) {
            CHECK(row.size() == system.prototiles().size());
        }
        for (const auto& seed : system.seeds()) {
            CHECK(system.generate_raw(seed.name, 0).size() == seed.patch.size());
        }
    }

    const auto row_sum = [](const auto& row) {
        return std::accumulate(row.begin(), row.end(), std::size_t{});
    };
    const auto& p1 = aper::tiling_system(aper::Tiling::p1);
    const auto p1_matrix = p1.rule().incidence_matrix(p1.prototiles().size());
    constexpr std::array<std::size_t, 6> p1_children{11, 6, 7, 7, 8, 3};
    for (std::size_t row = 0; row < p1_children.size(); ++row) {
        CHECK(row_sum(p1_matrix[row]) == p1_children[row]);
    }
    const auto& p2 = aper::tiling_system(aper::Tiling::p2);
    const auto p2_matrix = p2.rule().incidence_matrix(p2.prototiles().size());
    CHECK(row_sum(p2_matrix[0]) == 3);
    CHECK(row_sum(p2_matrix[1]) == 2);
    const auto& p3 = aper::tiling_system(aper::Tiling::p3);
    const auto p3_matrix = p3.rule().incidence_matrix(p3.prototiles().size());
    CHECK(row_sum(p3_matrix[0]) == 2);
    CHECK(row_sum(p3_matrix[1]) == 3);
    bool rejected_unrenderable_depth = false;
    try {
        (void)p3.generate("thin", 1);
    } catch (const std::invalid_argument&) {
        rejected_unrenderable_depth = true;
    }
    CHECK(rejected_unrenderable_depth);

    const auto& thue_morse = catalogue.get("thue-morse-2d");
    CHECK(thue_morse.spec().name == "Thue-Morse 2D");
    CHECK(thue_morse.prototiles().size() == 2);
    CHECK(thue_morse.rule().inflation() == 2.0);
    CHECK(thue_morse.rule().incidence_matrix(2) ==
          aper::IncidenceMatrix({{2, 2}, {2, 2}}));
    constexpr std::array<aper::PrototileId, 4> first_pattern{0, 1, 1, 0};
    constexpr std::array<aper::PrototileId, 4> second_pattern{1, 0, 0, 1};
    for (std::size_t i = 0; i < first_pattern.size(); ++i) {
        CHECK(thue_morse.rule().replacement(0).placements()[i].prototile ==
              first_pattern[i]);
        CHECK(thue_morse.rule().replacement(1).placements()[i].prototile ==
              second_pattern[i]);
    }
    CHECK(thue_morse.generate_raw("a", 3).size() == 64);
    CHECK(thue_morse.generate_raw("b", 3).size() == 64);
    CHECK((aper::GeometricValidator{{1.0e-9, 3, 512}}.validate(thue_morse).valid()));
    const auto thue_morse_rules = aper::RuleView{thue_morse}.drawing();
    CHECK(thue_morse_rules.polygons().size() == 10);
    CHECK(thue_morse_rules.arrows().size() == 2);
    CHECK(thue_morse_rules.metadata().title == "Thue-Morse 2D substitution rule");
    const auto thue_morse_patch = aper::PatchView{thue_morse, "a", 3}.drawing();
    CHECK(!thue_morse_patch.empty());
    CHECK(thue_morse_patch.polygons().size() <= 64);
    CHECK(close(thue_morse_patch.viewport().aspect_ratio(), 4.0 / 3.0));
    bool has_a_fill = false;
    bool has_b_fill = false;
    for (const auto& polygon : thue_morse_patch.polygons()) {
        has_a_fill |= polygon.fill.fill() == 0;
        has_b_fill |= polygon.fill.fill() == aper::maximum_fill;
    }
    CHECK(has_a_fill);
    CHECK(has_b_fill);
    CHECK(thue_morse_patch.arrows().empty());

    auto grid = make_square_system("grid", square_quarters());
    CHECK(grid.generate_raw("square", 2).size() == 16);
    aper::TilingCatalogue extended;
    extended.add(std::move(grid));
    CHECK(extended.get("grid").generate("square", 1).size() == 4);
    const auto* original_grid = extended.find("grid");
    extended.add(make_square_system("grid-2", square_quarters()));
    CHECK(extended.find("grid") == original_grid);

    const std::vector<aper::Point> square{
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    std::vector<aper::Prototile> misordered_prototiles{
        {1, "one", aper::Shape::square, square, 0},
        {0, "zero", aper::Shape::square, square, 0},
    };
    const auto half_size = aper::Similarity{{}, {0.5, 0.0}};
    std::vector<aper::RuleEntry> misordered_rules{
        {0, aper::Patch{{aper::Placement{0, half_size}}}},
        {1, aper::Patch{{aper::Placement{1, half_size}}}},
    };
    aper::TilingSystem invalid{
        {"misordered", "Misordered", {}, "zero", {1, 1, 1}},
        std::move(misordered_prototiles),
        aper::SubstitutionRule{2.0, std::move(misordered_rules)},
        {{"zero", aper::Patch{{aper::Placement{0, {}}}}, 1}},
        nullptr,
    };
    CHECK(invalid.validate().size() >= 2);
    bool rejected_invalid_generation = false;
    try {
        (void)invalid.generate("zero", 1);
    } catch (const std::logic_error&) {
        rejected_invalid_generation = true;
    }
    CHECK(rejected_invalid_generation);
    bool rejected_invalid_candidate = false;
    try {
        extended.add(std::move(invalid));
    } catch (const std::invalid_argument&) {
        rejected_invalid_candidate = true;
    }
    CHECK(rejected_invalid_candidate);

    auto incomplete_source = make_square_system("incomplete-source", square_quarters(),
                                                {{"aper", {}, {}, {}, {}}});
    CHECK(!incomplete_source.validate().empty());
    const aper::SourceReference local_source{
        "aper", "square-control", "https://github.com/njlane314/aper",
        "aper square control", "https://opensource.org/license/isc-license-txt"};
    auto duplicate_source = make_square_system("duplicate-source", square_quarters(),
                                               {local_source, local_source});
    CHECK(!duplicate_source.validate().empty());
}

bool has_issue(const aper::CandidateReport& report, aper::CandidateIssueKind kind) {
    return std::any_of(report.issues().begin(), report.issues().end(),
                       [kind](const auto& issue) { return issue.kind == kind; });
}

void test_discovery() {
    const aper::SquareLatticeSearch source;
    auto first_run = collect(source);
    auto second_run = collect(source);
    CHECK(first_run.size() == 1);
    CHECK(second_run.size() == 1);
    if (first_run.empty() || second_run.empty()) {
        return;
    }

    const auto& candidate = first_run.front();
    CHECK(candidate.validate().empty());
    CHECK(candidate.spec().id == "square-2x2");
    CHECK(candidate.spec().depths.minimum == 1);
    CHECK(candidate.spec().depths.recommended == 4);
    CHECK(candidate.spec().depths.maximum == 7);
    CHECK(candidate.prototiles().size() == 1);
    CHECK(candidate.rule().inflation() == 2.0);
    CHECK(candidate.rule().replacement(0).size() == 4);
    CHECK(candidate.seeds().size() == 1);
    CHECK(candidate.rule().incidence_matrix(1) == aper::IncidenceMatrix{{4}});
    CHECK(aper::area_eigenvalue_matches(candidate));
    CHECK(aper::incidence_is_primitive({{4}}));
    CHECK(!aper::incidence_is_primitive({{1, 0}, {0, 1}}));
    CHECK(aper::incidence_is_primitive({{1, 1}, {1, 0}}));

    const auto square_boundary = [](double side) {
        return std::vector<aper::Point>{
            {0.0, 0.0}, {side, 0.0}, {side, side}, {0.0, side}};
    };
    auto mixed_quarters = square_quarters();
    for (std::size_t i = 1; i < mixed_quarters.size(); ++i) {
        mixed_quarters[i].prototile = 1;
    }
    const aper::TilingSystem small_area_mismatch{
        {"small-area-mismatch", "Small area mismatch", {}, "first", {1, 1, 2}},
        {{0, "first", aper::Shape::generic_polygon, square_boundary(std::sqrt(2.0e-8)),
          0},
         {1, "second", aper::Shape::generic_polygon,
          square_boundary(std::sqrt(2.05e-8)), 1}},
        aper::SubstitutionRule{2.0,
                               {{0, aper::Patch{square_quarters()}},
                                {1, aper::Patch{std::move(mixed_quarters)}}}},
        {{"first", aper::Patch{{aper::Placement{0, {}}}}, 1}},
        aper::identity_projector(),
    };
    CHECK(small_area_mismatch.validate().empty());
    CHECK(!aper::area_eigenvalue_matches(small_area_mismatch));

    constexpr std::array expected_translations{
        aper::Point{0.0, 0.0},
        aper::Point{0.5, 0.0},
        aper::Point{0.0, 0.5},
        aper::Point{0.5, 0.5},
    };
    const auto placements = candidate.rule().replacement(0).placements();
    CHECK(placements.size() == expected_translations.size());
    for (std::size_t i = 0;
         i < std::min(placements.size(), expected_translations.size()); ++i) {
        CHECK(placements[i].prototile == 0);
        CHECK(placements[i].pose.translation() == expected_translations[i]);
        CHECK(placements[i].pose.multiplier() == aper::Point(0.5, 0.0));
        CHECK(!placements[i].pose.reflected());
    }

    std::size_t expected_count = 1;
    for (unsigned depth = 0; depth <= 7; ++depth) {
        CHECK(candidate.generate_raw("square", depth).size() == expected_count);
        expected_count *= 4;
    }

    const aper::GeometricValidator validator{{1.0e-9, 5, 2048}};
    CHECK(validator.validate(candidate).valid());
    const auto& known_penrose = aper::tiling_system(aper::Tiling::p3);
    CHECK(aper::area_eigenvalue_matches(known_penrose, 1.0e-8));
    const auto known_penrose_report =
        aper::GeometricValidator{{1.0e-8, 3, 256}}.validate(known_penrose);
    CHECK(known_penrose_report.valid());
    const aper::KnownTilingBank known_bank{aper::tiling_catalogue()};
    CHECK(known_bank.systems().size() == 7);
    const auto known_matches = known_bank.classify(known_penrose);
    CHECK(known_matches.size() == 1);
    if (!known_matches.empty()) {
        CHECK(known_matches.front().kind == aper::KnownMatchKind::exact_rule);
        CHECK(known_matches.front().system == &known_penrose);
    }
    CHECK(known_bank.classify(candidate).empty());
    bool rejected_bank_tolerance = false;
    try {
        (void)aper::KnownTilingBank{aper::tiling_catalogue(), 0.0};
    } catch (const std::invalid_argument&) {
        rejected_bank_tolerance = true;
    }
    CHECK(rejected_bank_tolerance);

    aper::TilingCatalogue local_bank_catalogue;
    local_bank_catalogue.add(make_square_system(
        "square-control", square_quarters(),
        {{"aper", "square-control", "https://github.com/njlane314/aper",
          "aper square control", "https://opensource.org/license/isc-license-txt"}}));
    const aper::KnownTilingBank local_bank{local_bank_catalogue};
    CHECK(local_bank.classify(make_reframed_square_system(true)).size() == 1);
    const auto resource_report =
        aper::GeometricValidator{{1.0e-9, 5, 3}}.validate(candidate);
    CHECK(!resource_report.valid());
    CHECK(has_issue(resource_report, aper::CandidateIssueKind::resource_limit));

    const auto discovery =
        aper::DiscoveryEngine{aper::DiscoveryOptions{{1.0e-9, 5, 2048}, 4, 4, true}}
            .run(source);
    CHECK(discovery.statistics.generated == 1);
    CHECK(discovery.statistics.structurally_valid == 1);
    CHECK(discovery.statistics.algebraically_valid == 1);
    CHECK(discovery.statistics.geometrically_valid == 1);
    CHECK(discovery.statistics.canonicalised == 1);
    CHECK(discovery.statistics.unique == 1);
    CHECK(discovery.candidates.size() == 1);
    if (!discovery.candidates.empty()) {
        CHECK(discovery.candidates.front().serialisation.starts_with(
            "aper-candidate-v2|"));
    }

    const auto deduplicated =
        aper::DiscoveryEngine{aper::DiscoveryOptions{{1.0e-9, 3, 256}, 4, 4, true}}.run(
            DuplicateSquareSource{});
    CHECK(deduplicated.statistics.generated == 2);
    CHECK(deduplicated.statistics.structurally_valid == 2);
    CHECK(deduplicated.statistics.algebraically_valid == 2);
    CHECK(deduplicated.statistics.geometrically_valid == 2);
    CHECK(deduplicated.statistics.canonicalised == 2);
    CHECK(deduplicated.statistics.unique == 1);
    CHECK(deduplicated.candidates.size() == 1);

    const auto bounded =
        aper::DiscoveryEngine{aper::DiscoveryOptions{{1.0e-9, 1, 16}, 1, 4, true}}.run(
            DuplicateSquareSource{});
    CHECK(bounded.statistics.generated == 1);
    CHECK(bounded.candidates.size() == 1);

    const auto first_key = aper::canonical_key(candidate);
    CHECK(first_key == aper::canonical_key(second_run.front()));
    auto reversed = square_quarters();
    std::reverse(reversed.begin(), reversed.end());
    const auto reordered = make_square_system("reordered", std::move(reversed));
    CHECK(first_key == aper::canonical_key(reordered));
    const auto reframed = make_reframed_square_system(false);
    const auto reflected = make_reframed_square_system(true);
    CHECK(validator.validate(reframed).valid());
    CHECK(validator.validate(reflected).valid());
    CHECK(first_key == aper::canonical_key(reframed));
    CHECK(first_key == aper::canonical_key(reflected));
    CHECK(aper::canonical_key(make_typed_square_system(false)) ==
          aper::canonical_key(make_typed_square_system(true)));
    CHECK(aper::canonical_key(make_typed_square_system(false)) ==
          aper::canonical_key(make_typed_square_system(false, true)));

    const aper::TilingSystem collinear{
        {"collinear", "Collinear grid", {}, "square", {1, 2, 7}},
        {{0,
          "square",
          aper::Shape::generic_polygon,
          {{0.0, 0.0}, {0.5, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
          0}},
        aper::SubstitutionRule{2.0, {{0, aper::Patch{square_quarters()}}}},
        {{"square", aper::Patch{{aper::Placement{0, {}}}}, 1}},
        aper::identity_projector(),
    };
    CHECK(validator.validate(collinear).valid());
    CHECK(first_key == aper::canonical_key(collinear));

    std::vector<aper::Prototile> many_prototiles;
    std::vector<aper::RuleEntry> many_rules;
    for (aper::PrototileId id = 0; id < 9; ++id) {
        many_prototiles.push_back({id,
                                   "type-" + std::to_string(id),
                                   aper::Shape::generic_polygon,
                                   {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
                                   0});
        auto many_placements = square_quarters();
        for (auto& placement : many_placements) {
            placement.prototile = id;
        }
        many_rules.push_back({id, aper::Patch{std::move(many_placements)}});
    }
    const aper::TilingSystem many_types{
        {"many-types", "Many types", {}, "type-0", {1, 1, 2}},
        std::move(many_prototiles),
        aper::SubstitutionRule{2.0, std::move(many_rules)},
        {{"type-0", aper::Patch{{aper::Placement{0, {}}}}, 1}},
        aper::identity_projector(),
    };
    CHECK(many_types.validate().empty());
    CHECK(aper::canonical_key(many_types).starts_with("aper-candidate-v2|types=9|"));

    const auto bent_square = [](double scale) {
        std::vector<aper::Placement> placements;
        for (const auto& placement : square_quarters()) {
            placements.push_back(
                {0, aper::Similarity{placement.pose.translation() * scale,
                                     placement.pose.multiplier()}});
        }
        return aper::TilingSystem{
            {"bent", "Bent grid", {}, "square", {1, 2, 7}},
            {{0,
              "square",
              aper::Shape::generic_polygon,
              {{0.0, 0.0},
               {0.5 * scale, 1.0e-8 * scale},
               {scale, 0.0},
               {scale, scale},
               {0.0, scale}},
              0}},
            aper::SubstitutionRule{2.0, {{0, aper::Patch{std::move(placements)}}}},
            {{"square", aper::Patch{{aper::Placement{0, {}}}}, 1}},
            aper::identity_projector(),
        };
    };
    const auto bent = bent_square(1.0);
    const auto small_bent = bent_square(1.0e-3);
    CHECK(bent.validate().empty());
    CHECK(small_bent.validate().empty());
    CHECK(aper::canonical_key(bent) == aper::canonical_key(small_bent));

    const auto numerical_report =
        aper::GeometricValidator{{1.0e-20, 1, 16}}.validate(candidate);
    CHECK(has_issue(numerical_report, aper::CandidateIssueKind::numerical_ambiguity));

    auto gap_placements = square_quarters();
    gap_placements.pop_back();
    const auto gap = make_square_system("gap", std::move(gap_placements));
    CHECK(gap.validate().empty());
    CHECK(!aper::area_eigenvalue_matches(gap));
    const auto gap_report = aper::GeometricValidator{{1.0e-9, 1, 16}}.validate(gap);
    CHECK(has_issue(gap_report, aper::CandidateIssueKind::area_mismatch));

    auto overlap_placements = square_quarters();
    overlap_placements.back() = overlap_placements.front();
    const auto overlap = make_square_system("overlap", std::move(overlap_placements));
    CHECK(overlap.validate().empty());
    CHECK(aper::area_eigenvalue_matches(overlap));
    const auto overlap_report =
        aper::GeometricValidator{{1.0e-9, 1, 16}}.validate(overlap);
    CHECK(has_issue(overlap_report, aper::CandidateIssueKind::overlap));

    auto outside_placements = square_quarters();
    outside_placements.back().pose = aper::Similarity{{1.0, 0.5}, {0.5, 0.0}};
    const auto outside = make_square_system("outside", std::move(outside_placements));
    CHECK(outside.validate().empty());
    const auto outside_report =
        aper::GeometricValidator{{1.0e-9, 1, 16}}.validate(outside);
    CHECK(has_issue(outside_report, aper::CandidateIssueKind::outside_parent));

    const std::vector<aper::Point> crossed_boundary{
        {0.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}, {2.0, 0.0}, {3.0, 1.0}};
    const aper::TilingSystem crossed{
        {"crossed", "Crossed", {}, "tile", {1, 1, 2}},
        {{0, "tile", aper::Shape::generic_polygon, crossed_boundary, 0}},
        aper::SubstitutionRule{2.0, {{0, aper::Patch{square_quarters()}}}},
        {{"tile", aper::Patch{{aper::Placement{0, {}}}}, 1}},
        aper::identity_projector(),
    };
    CHECK(crossed.validate().empty());
    const auto crossed_report =
        aper::GeometricValidator{{1.0e-9, 1, 16}}.validate(crossed);
    CHECK(has_issue(crossed_report, aper::CandidateIssueKind::non_simple_polygon));

    constexpr double crack = 7.5e-10;
    std::vector<aper::Placement> cracked_placements;
    for (const auto translation :
         std::array{aper::Point{0.0, 0.0}, aper::Point{0.5 + crack, 0.0},
                    aper::Point{0.0, 0.5}, aper::Point{0.5 + crack, 0.5}}) {
        cracked_placements.push_back({0, aper::Similarity{translation, {0.5, 0.0}}});
    }
    const auto cracked = make_square_system("cracked", std::move(cracked_placements));
    CHECK(cracked.validate().empty());
    const auto cracked_report =
        aper::GeometricValidator{{1.0e-9, 1, 16}}.validate(cracked);
    CHECK(has_issue(cracked_report, aper::CandidateIssueKind::edge_mismatch));

    const auto rules = aper::RuleView{candidate}.drawing();
    CHECK(rules.polygons().size() == 5);
    CHECK(rules.arrows().size() == 1);
    CHECK(rules.metadata().title == "Square 2x2 control substitution rule");
    const auto patch = aper::PatchView{candidate, "square", 2}.drawing();
    CHECK(patch.polygons().size() == 16);
    CHECK(patch.arrows().empty());
    CHECK(close(patch.viewport().aspect_ratio(), 4.0 / 3.0));

    std::ostringstream first_pdf;
    std::ostringstream second_pdf;
    aper::PdfRenderer{}.write(first_pdf, rules, aper::ColourScheme::tide);
    aper::PdfRenderer{}.write(second_pdf, rules, aper::ColourScheme::tide);
    CHECK(first_pdf.str() == second_pdf.str());
    CHECK(first_pdf.str().starts_with("%PDF-1.4\n"));
    CHECK(first_pdf.str().ends_with("%%EOF\n"));
    CHECK(first_pdf.str().find("/Creator (aper " + std::string(aper::version) + ")") !=
          std::string::npos);
    CHECK(first_pdf.str().find("nan") == std::string::npos);
    CHECK(first_pdf.str().find("inf") == std::string::npos);
}

void test_binary_square_search() {
    const aper::BinarySquareSearch source;
    const auto raw = collect(source);
    CHECK(raw.size() == 256);
    if (raw.size() != 256) {
        return;
    }

    CHECK(raw.front().spec().id == "binary-square-00-00");
    CHECK(raw.back().spec().id == "binary-square-15-15");
    const auto& at = [&](unsigned first, unsigned second) -> const aper::TilingSystem& {
        return raw[static_cast<std::size_t>(16 * first + second)];
    };

    std::size_t visited = 0;
    source.enumerate([&](aper::TilingSystem) {
        ++visited;
        return visited < 17;
    });
    CHECK(visited == 17);

    const auto reference = source.canonicalise(at(1, 3), 1.0e-9);
    constexpr std::array<std::array<unsigned, 2>, 8> symmetric_rules{{
        {{1, 3}},
        {{2, 10}},
        {{8, 12}},
        {{4, 5}},
        {{2, 3}},
        {{4, 12}},
        {{1, 5}},
        {{8, 10}},
    }};
    for (const auto& masks : symmetric_rules) {
        CHECK(source.canonicalise(at(masks[0], masks[1]), 1.0e-9) == reference);
    }
    CHECK(source.canonicalise(at(12, 14), 1.0e-9) == reference);
    CHECK(source.canonicalise(at(1, 10), 1.0e-9) != reference);
    CHECK(aper::canonical_key(at(1, 3), 1.0e-9) !=
          aper::canonical_key(at(1, 10), 1.0e-9));

    const auto thue_morse = source.canonicalise(at(6, 9), 1.0e-9);
    const auto parent_forgetting = source.canonicalise(at(6, 6), 1.0e-9);
    CHECK(thue_morse != parent_forgetting);
    CHECK(source.canonicalise(at(9, 6), 1.0e-9) == thue_morse);
    CHECK(aper::canonical_key(at(9, 6), 1.0e-9) ==
          aper::canonical_key(at(6, 9), 1.0e-9));
    CHECK(at(6, 9).rule().incidence_matrix(2) ==
          aper::IncidenceMatrix({{2, 2}, {2, 2}}));
    CHECK(at(6, 6).rule().incidence_matrix(2) ==
          aper::IncidenceMatrix({{2, 2}, {2, 2}}));
    const auto triangular = make_triangular_binary_system();
    CHECK(triangular.validate().empty());
    bool rejected_non_square = false;
    try {
        (void)source.canonicalise(triangular, 1.0e-9);
    } catch (const std::invalid_argument&) {
        rejected_non_square = true;
    }
    CHECK(rejected_non_square);

    const aper::DiscoveryOptions options{{1.0e-9, 3, 512}, 256, 64, true};
    const aper::DiscoveryEngine engine{options};
    const auto first = engine.run(source);
    const auto second = engine.run(source);
    CHECK(first.statistics.generated == 256);
    CHECK(first.statistics.structurally_valid == 256);
    CHECK(first.statistics.algebraically_valid == 224);
    CHECK(first.statistics.geometrically_valid == 224);
    CHECK(first.statistics.canonicalised == 224);
    CHECK(first.statistics.unique == 27);
    CHECK(first.candidates.size() == 27);
    CHECK(second.candidates.size() == first.candidates.size());

    std::set<std::string> serialisations;
    const aper::GeometricValidator validator{{1.0e-9, 3, 512}};
    for (std::size_t i = 0; i < first.candidates.size(); ++i) {
        const auto& candidate = first.candidates[i];
        CHECK(candidate.serialisation.starts_with("aper-binary-square-v1|"));
        CHECK(serialisations.insert(candidate.serialisation).second);
        CHECK(
            aper::incidence_is_primitive(candidate.system.rule().incidence_matrix(2)));
        CHECK(validator.validate(candidate.system).valid());
        CHECK(source.canonicalise(candidate.system, 1.0e-9) == candidate.serialisation);
        if (i < second.candidates.size()) {
            CHECK(second.candidates[i].serialisation == candidate.serialisation);
            CHECK(second.candidates[i].system.spec().id == candidate.system.spec().id);
        }
    }
    CHECK(serialisations.size() == 27);
    CHECK(serialisations.contains(thue_morse));
    CHECK(serialisations.contains(parent_forgetting));

    const auto thue_candidate = std::find_if(
        first.candidates.begin(), first.candidates.end(), [](const auto& value) {
            return value.system.spec().id == "binary-square-06-09";
        });
    const auto forgetting_candidate = std::find_if(
        first.candidates.begin(), first.candidates.end(), [](const auto& value) {
            return value.system.spec().id == "binary-square-06-06";
        });
    CHECK(thue_candidate != first.candidates.end());
    CHECK(forgetting_candidate != first.candidates.end());
    const aper::KnownTilingBank bank{aper::tiling_catalogue()};
    if (thue_candidate != first.candidates.end()) {
        const auto source_matches = bank.classify(thue_candidate->system, source);
        const auto generic_matches = bank.classify(thue_candidate->system);
        CHECK(source_matches.size() == 1);
        CHECK(generic_matches.size() == 1);
        if (!source_matches.empty()) {
            CHECK(source_matches.front().kind == aper::KnownMatchKind::exact_rule);
            CHECK(source_matches.front().system ==
                  &aper::tiling_catalogue().get("thue-morse-2d"));
        }
    }
    const auto reframed_matches = bank.classify(at(9, 6), source);
    CHECK(reframed_matches.size() == 1);
    if (!reframed_matches.empty()) {
        CHECK(reframed_matches.front().system ==
              &aper::tiling_catalogue().get("thue-morse-2d"));
    }
    if (forgetting_candidate != first.candidates.end()) {
        CHECK(bank.classify(forgetting_candidate->system).empty());
        CHECK(bank.classify(forgetting_candidate->system, source).empty());
    }
}

void check_seed(const SeedFixture& fixture) {
    auto triangles = aper::make_seed(fixture.tiling, fixture.seed);
    const auto acute = static_cast<std::size_t>(
        std::count_if(triangles.begin(), triangles.end(), [](const auto& triangle) {
            return triangle.kind == aper::TriangleKind::acute;
        }));
    CHECK(acute == fixture.acute);
    CHECK(triangles.size() - acute == fixture.obtuse);

    double expected_area = 0.0;
    for (const auto& triangle : triangles) {
        expected_area += area(triangle);
    }

    for (unsigned depth = 0; depth <= 5; ++depth) {
        CHECK(aper::predicted_triangle_count(fixture.tiling, fixture.seed, depth) ==
              triangles.size());

        double total_area = 0.0;
        for (const auto& triangle : triangles) {
            total_area += area(triangle);
        }
        CHECK(close(total_area, expected_area, 1.0e-8));

        const auto tiles = aper::pair_tiles(triangles, fixture.tiling);
        if (depth == 0) {
            CHECK(tiles.size() == fixture.initial_tiles);
        } else if (depth == 1) {
            CHECK(tiles.size() == fixture.next_tiles);
        } else {
            CHECK(!tiles.empty());
        }

        triangles = aper::subdivide(triangles, fixture.tiling);
    }
}

template <std::size_t Size>
void check_sun_tile_counts(aper::Tiling tiling,
                           const std::array<std::size_t, Size>& expected) {
    auto triangles = aper::make_seed(tiling, aper::Seed::sun);
    for (const auto count : expected) {
        CHECK(aper::pair_tiles(triangles, tiling).size() == count);
        triangles = aper::subdivide(triangles, tiling);
    }
}

void test_seed_counts_and_area() {
    constexpr std::array fixtures{
        SeedFixture{aper::Tiling::p2, aper::Seed::sun, 10, 0, 5, 15},
        SeedFixture{aper::Tiling::p2, aper::Seed::star, 0, 10, 5, 10},
        SeedFixture{aper::Tiling::p2, aper::Seed::ace, 4, 2, 3, 6},
        SeedFixture{aper::Tiling::p2, aper::Seed::deuce, 4, 4, 4, 7},
        SeedFixture{aper::Tiling::p2, aper::Seed::jack, 6, 4, 5, 11},
        SeedFixture{aper::Tiling::p2, aper::Seed::queen, 8, 2, 5, 12},
        SeedFixture{aper::Tiling::p2, aper::Seed::king, 4, 6, 5, 11},
        SeedFixture{aper::Tiling::p3, aper::Seed::sun, 10, 0, 0, 10},
        SeedFixture{aper::Tiling::p3, aper::Seed::star, 0, 10, 5, 10},
        SeedFixture{aper::Tiling::p3, aper::Seed::thin, 2, 0, 1, 0},
        SeedFixture{aper::Tiling::p3, aper::Seed::thick, 0, 2, 1, 1},
    };

    for (const auto& fixture : fixtures) {
        check_seed(fixture);
    }

    check_sun_tile_counts(aper::Tiling::p2, std::array<std::size_t, 8>{
                                                5, 15, 35, 95, 265, 705, 1855, 4885});
    check_sun_tile_counts(aper::Tiling::p3, std::array<std::size_t, 8>{
                                                0, 10, 20, 60, 160, 430, 1140, 3010});

    CHECK(aper::predicted_triangle_count(aper::Tiling::p2, aper::Seed::sun, 12) ==
          1213930);
    CHECK(aper::predicted_triangle_count(aper::Tiling::p3, aper::Seed::sun, 12) ==
          750250);

    CHECK(!aper::seed_supported(aper::Tiling::p2, aper::Seed::thin));
    CHECK(!aper::seed_supported(aper::Tiling::p3, aper::Seed::ace));
    CHECK(!aper::seed_supported(aper::Tiling::p1, aper::Seed::sun));
    CHECK(aper::seed_supported(aper::Tiling::p1, aper::Seed::pentagon_5));
    CHECK(aper::seed_supported(aper::Tiling::p1, aper::Seed::diamond));
    CHECK(aper::seed_supported(aper::Tiling::p1, aper::Seed::boat));
    CHECK(aper::seed_supported(aper::Tiling::p1, aper::Seed::star));
    CHECK(aper::seed_supported(aper::Tiling::p2, aper::Seed::queen));
    CHECK(aper::seed_supported(aper::Tiling::p3, aper::Seed::thick));
    CHECK(aper::seed_supported(aper::Tiling::ammann_beenker, aper::Seed::octagon));
    CHECK(aper::seed_supported(aper::Tiling::pinwheel, aper::Seed::triangle));
    CHECK(aper::seed_supported(aper::Tiling::stampfli, aper::Seed::dodecagon));
    CHECK(!aper::seed_supported(aper::Tiling::ammann_beenker, aper::Seed::triangle));
    CHECK(!aper::seed_supported(aper::Tiling::pinwheel, aper::Seed::square));
    CHECK(!aper::seed_supported(aper::Tiling::stampfli, aper::Seed::octagon));
}

int interior_angle(const aper::Tile& tile, std::size_t vertex) {
    const auto point = tile.vertices[vertex];
    const auto incoming = point - tile.vertices[(vertex + 3) % tile.vertices.size()];
    const auto outgoing = tile.vertices[(vertex + 1) % tile.vertices.size()] - point;
    const auto turn_angle =
        std::atan2(cross(incoming, outgoing), incoming.real() * outgoing.real() +
                                                  incoming.imag() * outgoing.imag());
    return static_cast<int>(
        std::lround((std::numbers::pi - turn_angle) * 180.0 / std::numbers::pi));
}

std::string centre_signature(aper::Seed seed) {
    const auto triangles = aper::make_seed(aper::Tiling::p2, seed);
    const auto tiles = aper::pair_tiles(triangles, aper::Tiling::p2);
    std::vector<std::string> tokens;

    for (const auto& tile : tiles) {
        for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
            if (std::abs(tile.vertices[i]) > 1.0e-9) {
                continue;
            }
            tokens.push_back(std::string(tile.shape == aper::Shape::kite ? "K" : "D") +
                             std::to_string(interior_angle(tile, i)));
        }
    }

    std::sort(tokens.begin(), tokens.end());
    std::ostringstream signature;
    for (const auto& token : tokens) {
        if (signature.tellp() != 0) {
            signature << ' ';
        }
        signature << token;
    }
    return signature.str();
}

void test_p2_vertex_seeds() {
    CHECK(centre_signature(aper::Seed::sun) == "K72 K72 K72 K72 K72");
    CHECK(centre_signature(aper::Seed::star) == "D72 D72 D72 D72 D72");
    CHECK(centre_signature(aper::Seed::ace) == "D216 K72 K72");
    CHECK(centre_signature(aper::Seed::deuce) == "D36 D36 K144 K144");
    CHECK(centre_signature(aper::Seed::jack) == "D36 D36 K144 K72 K72");
    CHECK(centre_signature(aper::Seed::queen) == "D72 K72 K72 K72 K72");
    CHECK(centre_signature(aper::Seed::king) == "D72 D72 D72 K72 K72");
}

void test_largest_component() {
    constexpr std::array p2_seeds{
        aper::Seed::sun,  aper::Seed::star,  aper::Seed::ace,  aper::Seed::deuce,
        aper::Seed::jack, aper::Seed::queen, aper::Seed::king,
    };
    bool removed_island = false;
    for (const auto seed : p2_seeds) {
        const auto triangles = aper::generate(aper::Tiling::p2, seed, 5);
        const auto paired = aper::pair_tiles(triangles, aper::Tiling::p2);
        const auto connected = aper::largest_component(paired);
        CHECK(!connected.empty());
        CHECK(connected.size() <= paired.size());
        CHECK(aper::largest_component(connected).size() == connected.size());
        removed_island = removed_island || connected.size() < paired.size();
    }
    CHECK(removed_island);

    constexpr std::array p3_seeds{
        aper::Seed::sun,
        aper::Seed::star,
        aper::Seed::thin,
        aper::Seed::thick,
    };
    for (const auto seed : p3_seeds) {
        const auto triangles = aper::generate(aper::Tiling::p3, seed, 5);
        const auto paired = aper::pair_tiles(triangles, aper::Tiling::p3);
        const auto connected = aper::largest_component(paired);
        CHECK(!connected.empty());
        CHECK(connected.size() <= paired.size());
        CHECK(aper::largest_component(connected).size() == connected.size());
    }
}

void test_rhomb_geometry() {
    const auto triangles = aper::generate(aper::Tiling::p3, aper::Seed::sun, 7);
    const auto tiles = aper::pair_tiles(triangles, aper::Tiling::p3);
    CHECK(!tiles.empty());

    for (const auto& tile : tiles) {
        std::array<double, 4> sides{};
        for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
            sides[i] = std::abs(tile.vertices[(i + 1) % tile.vertices.size()] -
                                tile.vertices[i]);
        }
        for (const auto side : sides) {
            CHECK(close(side, sides.front(), 1.0e-9));
        }
        CHECK(signed_area(tile) > 0.0);
    }
}

void test_kite_and_dart_geometry() {
    const auto triangles = aper::generate(aper::Tiling::p2, aper::Seed::sun, 5);
    const auto tiles = aper::pair_tiles(triangles, aper::Tiling::p2);
    CHECK(!tiles.empty());

    double kite_area = 0.0;
    double dart_area = 0.0;
    std::size_t kites = 0;
    std::size_t darts = 0;

    for (const auto& tile : tiles) {
        std::array<double, 4> sides{};
        unsigned reflex_turns = 0;
        for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
            sides[i] = std::abs(tile.vertices[(i + 1) % tile.vertices.size()] -
                                tile.vertices[i]);
            if (turn(tile.vertices[(i + 3) % tile.vertices.size()], tile.vertices[i],
                     tile.vertices[(i + 1) % tile.vertices.size()]) < 0.0) {
                ++reflex_turns;
            }
        }
        std::sort(sides.begin(), sides.end());
        CHECK(close(sides[0], sides[1], 1.0e-9));
        CHECK(close(sides[2], sides[3], 1.0e-9));
        CHECK(close(sides[2] / sides[0], std::numbers::phi, 1.0e-8));
        CHECK(signed_area(tile) > 0.0);

        if (tile.shape == aper::Shape::kite) {
            ++kites;
            kite_area = signed_area(tile);
            CHECK(reflex_turns == 0);
        } else {
            CHECK(tile.shape == aper::Shape::dart);
            ++darts;
            dart_area = signed_area(tile);
            CHECK(reflex_turns == 1);
        }
    }

    CHECK(kites == 440);
    CHECK(darts == 265);
    CHECK(close(kite_area / dart_area, std::numbers::phi, 1.0e-8));
}

void test_p1_geometry() {
    struct Fixture {
        aper::Seed seed;
        std::size_t vertices;
        unsigned reflex_vertices;
    };
    constexpr std::array fixtures{
        Fixture{aper::Seed::pentagon_5, 5, 0}, Fixture{aper::Seed::pentagon_3, 5, 0},
        Fixture{aper::Seed::pentagon_2, 5, 0}, Fixture{aper::Seed::diamond, 4, 0},
        Fixture{aper::Seed::boat, 7, 2},       Fixture{aper::Seed::star, 10, 5},
    };

    for (const auto& fixture : fixtures) {
        const auto tiles = aper::generate_tiles(aper::Tiling::p1, fixture.seed, 0);
        CHECK(tiles.size() == 1);
        const auto& tile = tiles.front();
        CHECK(tile.vertices.size() == fixture.vertices);
        CHECK(signed_area(tile) > 0.0);

        unsigned reflex_vertices = 0;
        for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
            const auto side = std::abs(tile.vertices[(i + 1) % tile.vertices.size()] -
                                       tile.vertices[i]);
            CHECK(close(side, 1.0, 1.0e-9));
            if (turn(tile.vertices[(i + tile.vertices.size() - 1) %
                                   tile.vertices.size()],
                     tile.vertices[i],
                     tile.vertices[(i + 1) % tile.vertices.size()]) < 0.0) {
                ++reflex_vertices;
            }
        }
        CHECK(reflex_vertices == fixture.reflex_vertices);
    }

    const auto children =
        aper::generate_tiles(aper::Tiling::p1, aper::Seed::pentagon_5, 1);
    for (const auto& tile : children) {
        for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
            CHECK(close(std::abs(tile.vertices[(i + 1) % tile.vertices.size()] -
                                 tile.vertices[i]),
                        1.0 / (std::numbers::phi * std::numbers::phi), 1.0e-9));
        }
    }
}

std::size_t count_shape(std::span<const aper::Tile> tiles, aper::Shape shape) {
    return static_cast<std::size_t>(
        std::count_if(tiles.begin(), tiles.end(),
                      [shape](const auto& tile) { return tile.shape == shape; }));
}

void check_convex_tiles(std::span<const aper::Tile> tiles) {
    for (const auto& tile : tiles) {
        CHECK(tile.vertices.size() >= 3);
        CHECK(signed_area(tile) > 0.0);
        CHECK(tile.fill <= aper::maximum_fill);
        for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
            const auto previous =
                tile.vertices[(i + tile.vertices.size() - 1) % tile.vertices.size()];
            const auto current = tile.vertices[i];
            const auto next = tile.vertices[(i + 1) % tile.vertices.size()];
            CHECK(turn(previous, current, next) > -1.0e-10);
        }
    }
}

void test_ammann_beenker() {
    struct Counts {
        std::size_t squares;
        std::size_t rhombs;
    };
    constexpr std::array square_counts{
        Counts{1, 0},
        Counts{1, 4},
        Counts{13, 24},
        Counts{89, 140},
    };
    constexpr std::array rhomb_counts{
        Counts{0, 1},
        Counts{0, 3},
        Counts{8, 17},
        Counts{60, 99},
    };

    const auto check = [](aper::Seed seed, std::span<const Counts> expected) {
        const auto inflation = 1.0 + std::sqrt(2.0);
        for (unsigned depth = 0; depth < expected.size(); ++depth) {
            const auto tiles =
                aper::generate_tiles(aper::Tiling::ammann_beenker, seed, depth);
            CHECK(count_shape(tiles, aper::Shape::square) == expected[depth].squares);
            CHECK(count_shape(tiles, aper::Shape::ammann_rhomb) ==
                  expected[depth].rhombs);
            CHECK(tiles.size() == expected[depth].squares + expected[depth].rhombs);
            check_convex_tiles(tiles);

            const auto expected_edge = std::pow(inflation, -static_cast<double>(depth));
            for (const auto& tile : tiles) {
                for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
                    CHECK(close(std::abs(tile.vertices[(i + 1) % tile.vertices.size()] -
                                         tile.vertices[i]),
                                expected_edge, 1.0e-8));
                }
            }
        }
    };
    check(aper::Seed::square, square_counts);
    check(aper::Seed::rhomb, rhomb_counts);

    const auto octagon =
        aper::generate_tiles(aper::Tiling::ammann_beenker, aper::Seed::octagon, 1);
    CHECK(octagon.size() == 32);
    CHECK(count_shape(octagon, aper::Shape::square) == 8);
    CHECK(count_shape(octagon, aper::Shape::ammann_rhomb) == 24);
    check_convex_tiles(octagon);
}

void test_pinwheel() {
    std::size_t expected_count = 1;
    for (unsigned depth = 0; depth <= 6; ++depth) {
        const auto tiles =
            aper::generate_tiles(aper::Tiling::pinwheel, aper::Seed::triangle, depth);
        CHECK(tiles.size() == expected_count);
        check_convex_tiles(tiles);

        double total_area = 0.0;
        std::array<bool, aper::maximum_fill + 1> fills{};
        for (const auto& tile : tiles) {
            CHECK(tile.shape == aper::Shape::pinwheel_triangle);
            total_area += signed_area(tile);
            fills[tile.fill] = true;

            std::array<double, 3> sides{};
            for (std::size_t i = 0; i < sides.size(); ++i) {
                sides[i] =
                    std::abs(tile.vertices[(i + 1) % sides.size()] - tile.vertices[i]);
            }
            std::sort(sides.begin(), sides.end());
            CHECK(close(sides[1] / sides[0], 2.0, 1.0e-8));
            CHECK(close(sides[2] / sides[0], std::sqrt(5.0), 1.0e-8));
        }
        CHECK(close(total_area, 1.0, 1.0e-8));
        if (depth == 6) {
            CHECK(std::count(fills.begin(), fills.end(), true) >= 3);
        }
        expected_count *= 5;
    }
}

void test_stampfli() {
    struct Fixture {
        aper::Seed seed;
        std::size_t initial;
        std::size_t triangles;
        std::size_t rhombs;
        std::size_t squares;
    };
    constexpr std::array fixtures{
        Fixture{aper::Seed::triangle, 1, 10, 6, 0},
        Fixture{aper::Seed::rhomb, 1, 12, 3, 2},
        Fixture{aper::Seed::square, 1, 20, 12, 1},
        Fixture{aper::Seed::dodecagon, 12, 120, 36, 24},
    };

    for (const auto& fixture : fixtures) {
        const auto seed = aper::generate_tiles(aper::Tiling::stampfli, fixture.seed, 0);
        CHECK(seed.size() == fixture.initial);
        check_convex_tiles(seed);

        const auto children =
            aper::generate_tiles(aper::Tiling::stampfli, fixture.seed, 1);
        CHECK(count_shape(children, aper::Shape::equilateral_triangle) ==
              fixture.triangles);
        CHECK(count_shape(children, aper::Shape::stampfli_rhomb) == fixture.rhombs);
        CHECK(count_shape(children, aper::Shape::square) == fixture.squares);
        CHECK(children.size() == fixture.triangles + fixture.rhombs + fixture.squares);
        check_convex_tiles(children);

        for (const auto& tile : children) {
            const auto first_edge = std::abs(tile.vertices[1] - tile.vertices[0]);
            for (std::size_t i = 1; i < tile.vertices.size(); ++i) {
                CHECK(close(std::abs(tile.vertices[(i + 1) % tile.vertices.size()] -
                                     tile.vertices[i]),
                            first_edge, 1.0e-8));
            }
        }
    }

    const auto deeper =
        aper::generate_tiles(aper::Tiling::stampfli, aper::Seed::dodecagon, 2);
    CHECK(deeper.size() == 2820);
    check_convex_tiles(deeper);
}

struct ColourFixture {
    aper::ColourScheme scheme;
    std::string_view name;
    std::string_view primary;
    std::string_view secondary;
};

std::string check_pdf(aper::Tiling tiling, aper::Seed seed, std::string_view title,
                      const ColourFixture& colours, unsigned depth = 3,
                      bool expect_secondary = true) {
    const auto tiles = aper::generate_tiles(tiling, seed, depth);

    std::ostringstream first;
    std::ostringstream second;
    aper::write_pdf(first, tiles, tiling, seed, colours.scheme, depth);
    aper::write_pdf(second, tiles, tiling, seed, colours.scheme, depth);

    const auto pdf = first.str();
    CHECK(pdf == second.str());
    CHECK(pdf.starts_with("%PDF-1.4\n"));
    CHECK(pdf.ends_with("%%EOF\n"));
    CHECK(pdf.find(title) != std::string::npos);
    CHECK(pdf.find("/Creator (aper " + std::string(aper::version) + ")") !=
          std::string::npos);
    CHECK(pdf.find("/MediaBox [0 0 720 720]") != std::string::npos);
    CHECK(pdf.find("/Resources << >>") != std::string::npos);
    CHECK(pdf.find("/Subject (" + std::string(aper::seed_name(seed)) +
                   " seed at depth " + std::to_string(depth) + "; " +
                   std::string(colours.name) + " colour scheme)") != std::string::npos);
    CHECK(pdf.find("1.0000 1.0000 1.0000 rg\n0 0 720.0000 720.0000 re f") !=
          std::string::npos);
    CHECK(pdf.find("0.0627 0.0863 0.1059 RG") != std::string::npos);
    CHECK(pdf.find(colours.primary) != std::string::npos);
    if (expect_secondary) {
        CHECK(pdf.find(colours.secondary) != std::string::npos);
    }
    CHECK(pdf.find("nan") == std::string::npos);
    CHECK(pdf.find("inf") == std::string::npos);
    CHECK(occurrences(pdf, " m\n") == tiles.size());

    const auto length_marker = pdf.find("/Length ");
    const auto length_start = length_marker + std::string_view{"/Length "}.size();
    const auto length_end = pdf.find(' ', length_start);
    const auto stream_marker = pdf.find("stream\n", length_end);
    const auto stream_start = stream_marker + std::string_view{"stream\n"}.size();
    const auto stream_end = pdf.find("endstream", stream_start);
    CHECK(length_marker != std::string::npos);
    CHECK(length_end != std::string::npos);
    CHECK(stream_marker != std::string::npos);
    CHECK(stream_end != std::string::npos);
    if (length_marker != std::string::npos && length_end != std::string::npos &&
        stream_marker != std::string::npos && stream_end != std::string::npos) {
        CHECK(std::stoull(pdf.substr(length_start, length_end - length_start)) ==
              stream_end - stream_start);
    }

    const auto xref = pdf.find("xref\n");
    CHECK(xref != std::string::npos);
    CHECK(pdf.find("startxref\n" + std::to_string(xref) + "\n%%EOF\n") !=
          std::string::npos);
    for (unsigned object = 1; object <= 5; ++object) {
        const auto marker = std::to_string(object) + " 0 obj\n";
        const auto offset = pdf.find(marker);
        std::ostringstream encoded;
        encoded << std::setfill('0') << std::setw(10) << offset << " 00000 n \n";
        CHECK(offset != std::string::npos);
        CHECK(pdf.find(encoded.str(), xref) != std::string::npos);
    }

    return pdf;
}

void test_pdf() {
    constexpr std::array schemes{
        ColourFixture{aper::ColourScheme::flare, "flare", "0.1922 0.3412 0.8353",
                      "1.0000 0.3882 0.2392"},
        ColourFixture{aper::ColourScheme::grove, "grove", "0.0000 0.5412 0.3529",
                      "1.0000 0.7569 0.2706"},
        ColourFixture{aper::ColourScheme::electric, "electric", "0.4431 0.2157 0.7843",
                      "0.7059 0.8392 0.0000"},
        ColourFixture{aper::ColourScheme::tide, "tide", "0.8471 0.1059 0.3765",
                      "0.0000 0.6549 0.7686"},
    };

    constexpr std::array p1_seeds{
        aper::Seed::pentagon_5, aper::Seed::pentagon_3, aper::Seed::pentagon_2,
        aper::Seed::diamond,    aper::Seed::boat,       aper::Seed::star,
    };
    for (const auto seed : p1_seeds) {
        const auto pdf = check_pdf(aper::Tiling::p1, seed,
                                   "/Title (P1 Penrose tiling - " +
                                       std::string(aper::seed_name(seed)) + ")",
                                   schemes.front());
        CHECK(!pdf.empty());
    }

    std::string previous;
    for (const auto& colours : schemes) {
        const auto pdf = check_pdf(aper::Tiling::p2, aper::Seed::sun,
                                   "/Title (P2 Penrose tiling - sun)", colours);
        if (!previous.empty()) {
            CHECK(pdf != previous);
        }
        previous = pdf;
    }

    constexpr std::array p2_seeds{
        aper::Seed::star, aper::Seed::ace,   aper::Seed::deuce,
        aper::Seed::jack, aper::Seed::queen, aper::Seed::king,
    };
    for (const auto seed : p2_seeds) {
        const auto pdf = check_pdf(aper::Tiling::p2, seed,
                                   "/Title (P2 Penrose tiling - " +
                                       std::string(aper::seed_name(seed)) + ")",
                                   schemes.front());
        CHECK(!pdf.empty());
    }

    constexpr std::array p3_seeds{
        aper::Seed::sun,
        aper::Seed::star,
        aper::Seed::thin,
        aper::Seed::thick,
    };
    for (const auto seed : p3_seeds) {
        const auto pdf = check_pdf(aper::Tiling::p3, seed,
                                   "/Title (P3 Penrose tiling - " +
                                       std::string(aper::seed_name(seed)) + ")",
                                   schemes.front());
        CHECK(!pdf.empty());
    }

    CHECK(!check_pdf(aper::Tiling::ammann_beenker, aper::Seed::octagon,
                     "/Title (Ammann-Beenker tiling - octagon)", schemes.front(), 2)
               .empty());
    const auto pinwheel =
        check_pdf(aper::Tiling::pinwheel, aper::Seed::triangle,
                  "/Title (Pinwheel tiling - triangle)", schemes.front(), 4, false);
    CHECK(!pinwheel.empty());
    CHECK(occurrences(pinwheel, " rg\n") >= 4);
    CHECK(!check_pdf(aper::Tiling::stampfli, aper::Seed::dodecagon,
                     "/Title (Stampfli 12-fold 1 tiling - dodecagon)", schemes.front(),
                     1)
               .empty());
}

void test_invalid_pdf_geometry() {
    const auto rejected = [](std::vector<aper::Tile> tiles) {
        std::ostringstream output;
        try {
            aper::write_pdf(output, tiles, aper::Tiling::p1, aper::Seed::pentagon_5,
                            aper::ColourScheme::flare, 1);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };

    CHECK(rejected({}));
    CHECK(rejected({{aper::Shape::pentagon_5, {}}}));
    CHECK(
        rejected({{aper::Shape::pentagon_5, {aper::Point{}, aper::Point{1.0, 0.0}}}}));
    CHECK(rejected({{aper::Shape::pentagon_5,
                     {aper::Point{}, aper::Point{1.0, 0.0}, aper::Point{2.0, 0.0}}}}));
    CHECK(rejected(
        {{aper::Shape::pentagon_5,
          {aper::Point{}, aper::Point{std::numeric_limits<double>::quiet_NaN(), 0.0},
           aper::Point{0.0, 1.0}}}}));
    CHECK(rejected({{aper::Shape::pentagon_5,
                     {aper::Point{}, aper::Point{1.0, 0.0}, aper::Point{0.0, 1.0}},
                     static_cast<std::uint8_t>(aper::maximum_fill + 1)}}));
}

void test_patch_and_rule_views() {
    for (const auto& system : aper::tiling_catalogue().systems()) {
        const auto& seed = system.seed(system.spec().default_seed);
        const auto depth = std::max(system.spec().depths.minimum, seed.minimum_depth);
        const auto patch = aper::PatchView{system, seed.name, depth}.drawing();
        CHECK(close(patch.viewport().aspect_ratio(), 4.0 / 3.0));
        CHECK(!patch.polygons().empty());
        CHECK(patch.arrows().empty());
        CHECK(patch.metadata().title.find(system.spec().name) != std::string::npos);
        for (const auto& polygon : patch.polygons()) {
            CHECK(patch.viewport().intersects(polygon.vertices));
        }

        const auto rules = aper::RuleView{system}.drawing();
        std::size_t replacement_count = 0;
        for (const auto& entry : system.rule().entries()) {
            replacement_count += entry.replacement.size();
        }
        CHECK(close(rules.viewport().aspect_ratio(), 4.0 / 3.0));
        CHECK(rules.polygons().size() ==
              system.prototiles().size() + replacement_count);
        CHECK(rules.arrows().size() == system.rule().entries().size());
        CHECK(rules.metadata().title == system.spec().name + " substitution rule");

        std::ostringstream first;
        std::ostringstream second;
        aper::PdfRenderer{}.write(first, rules, aper::ColourScheme::grove);
        aper::PdfRenderer{}.write(second, rules, aper::ColourScheme::grove);
        const auto pdf = first.str();
        CHECK(pdf == second.str());
        CHECK(pdf.starts_with("%PDF-1.4\n"));
        CHECK(pdf.ends_with("%%EOF\n"));
        CHECK(pdf.find("/MediaBox [0 0 720.0000 540.0000]") != std::string::npos);
        CHECK(pdf.find("/Title (" + rules.metadata().title + ")") != std::string::npos);
        CHECK(pdf.find("/Creator (aper " + std::string(aper::version) + ")") !=
              std::string::npos);
        CHECK(pdf.find("nan") == std::string::npos);
        CHECK(pdf.find("inf") == std::string::npos);
    }
}

} // namespace

int main() {
    test_substitution_systems();
    test_discovery();
    test_binary_square_search();
    test_seed_counts_and_area();
    test_p1_substitution();
    test_p2_vertex_seeds();
    test_largest_component();
    test_p1_geometry();
    test_rhomb_geometry();
    test_kite_and_dart_geometry();
    test_ammann_beenker();
    test_pinwheel();
    test_stampfli();
    test_pdf();
    test_invalid_pdf_geometry();
    test_patch_and_rule_views();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "all tests passed\n";
    return EXIT_SUCCESS;
}
