#include "pdf.hpp"
#include "penrose.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            std::cerr << __FILE__ << ':' << __LINE__                                   \
                      << ": check failed: " #condition "\n";                           \
            ++failures;                                                                \
        }                                                                              \
    } while (false)

[[nodiscard]] bool close(double lhs, double rhs, double tolerance = 1.0e-9) {
    return std::abs(lhs - rhs) <= tolerance;
}

[[nodiscard]] double cross(aper::Point a, aper::Point b) {
    return a.real() * b.imag() - a.imag() * b.real();
}

[[nodiscard]] double area(const aper::RobinsonTriangle& triangle) {
    return std::abs(cross(triangle.base_a - triangle.apex,
                          triangle.base_b - triangle.apex)) /
           2.0;
}

[[nodiscard]] double signed_area(const aper::Tile& tile) {
    double twice_area = 0.0;
    for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
        twice_area +=
            cross(tile.vertices[i], tile.vertices[(i + 1) % tile.vertices.size()]);
    }
    return twice_area / 2.0;
}

[[nodiscard]] double turn(aper::Point a, aper::Point b, aper::Point c) {
    return cross(b - a, c - b);
}

[[nodiscard]] std::size_t occurrences(const std::string& text,
                                      std::string_view needle) {
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
    CHECK(aper::seed_supported(aper::Tiling::p2, aper::Seed::queen));
    CHECK(aper::seed_supported(aper::Tiling::p3, aper::Seed::thick));
}

[[nodiscard]] int interior_angle(const aper::Tile& tile, std::size_t vertex) {
    const auto point = tile.vertices[vertex];
    const auto incoming = point - tile.vertices[(vertex + 3) % tile.vertices.size()];
    const auto outgoing = tile.vertices[(vertex + 1) % tile.vertices.size()] - point;
    const auto turn_angle =
        std::atan2(cross(incoming, outgoing), incoming.real() * outgoing.real() +
                                                  incoming.imag() * outgoing.imag());
    return static_cast<int>(
        std::lround((std::numbers::pi - turn_angle) * 180.0 / std::numbers::pi));
}

[[nodiscard]] std::string centre_signature(aper::Seed seed) {
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

struct ColourFixture {
    aper::ColourScheme scheme;
    std::string_view name;
    std::string_view primary;
    std::string_view secondary;
};

[[nodiscard]] std::string check_pdf(aper::Tiling tiling, aper::Seed seed,
                                    std::string_view title,
                                    const ColourFixture& colours) {
    const auto triangles = aper::generate(tiling, seed, 3);
    const auto paired = aper::pair_tiles(triangles, tiling);
    const auto tiles = aper::largest_component(paired);

    std::ostringstream first;
    std::ostringstream second;
    aper::write_pdf(first, tiles, tiling, seed, colours.scheme, 3);
    aper::write_pdf(second, tiles, tiling, seed, colours.scheme, 3);

    const auto pdf = first.str();
    CHECK(pdf == second.str());
    CHECK(pdf.starts_with("%PDF-1.4\n"));
    CHECK(pdf.ends_with("%%EOF\n"));
    CHECK(pdf.find(title) != std::string::npos);
    CHECK(pdf.find("/Creator (aper 0.4.0)") != std::string::npos);
    CHECK(pdf.find("/MediaBox [0 0 720 720]") != std::string::npos);
    CHECK(pdf.find("/Resources << >>") != std::string::npos);
    CHECK(pdf.find("/Subject (" + std::string(aper::seed_name(seed)) +
                   " seed at depth 3; " + std::string(colours.name) +
                   " colour scheme)") != std::string::npos);
    CHECK(pdf.find("1.0000 1.0000 1.0000 rg\n0 0 720.0000 720.0000 re f") !=
          std::string::npos);
    CHECK(pdf.find("0.0627 0.0863 0.1059 RG") != std::string::npos);
    CHECK(pdf.find(colours.primary) != std::string::npos);
    CHECK(pdf.find(colours.secondary) != std::string::npos);
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
}

} // namespace

int main() {
    test_seed_counts_and_area();
    test_p2_vertex_seeds();
    test_largest_component();
    test_rhomb_geometry();
    test_kite_and_dart_geometry();
    test_pdf();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "all tests passed\n";
    return EXIT_SUCCESS;
}
