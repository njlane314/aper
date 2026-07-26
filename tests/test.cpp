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

template <std::size_t Size>
void check_counts_and_area(aper::Tiling tiling,
                           const std::array<std::size_t, Size>& triangle_counts,
                           const std::array<std::size_t, Size>& tile_counts) {
    auto triangles = aper::sun_seed();
    const auto expected_area = 5.0 * std::sin(std::numbers::pi / 5.0);

    for (unsigned depth = 0; depth < triangle_counts.size(); ++depth) {
        CHECK(triangles.size() == triangle_counts[depth]);
        CHECK(aper::predicted_triangle_count(tiling, depth) == triangle_counts[depth]);

        double total_area = 0.0;
        for (const auto& triangle : triangles) {
            total_area += area(triangle);
        }
        CHECK(close(total_area, expected_area, 1.0e-8));

        const auto tiles = aper::pair_tiles(triangles, tiling);
        CHECK(tiles.size() == tile_counts[depth]);

        if (depth + 1 < triangle_counts.size()) {
            triangles = aper::subdivide(triangles, tiling);
        }
    }
}

void test_counts_and_area() {
    constexpr std::array<std::size_t, 8> p2_triangle_counts{
        10, 30, 80, 210, 550, 1440, 3770, 9870,
    };
    constexpr std::array<std::size_t, 8> p2_tile_counts{
        5, 15, 35, 95, 265, 705, 1855, 4885,
    };
    constexpr std::array<std::size_t, 8> p3_triangle_counts{
        10, 20, 50, 130, 340, 890, 2330, 6100,
    };
    constexpr std::array<std::size_t, 8> p3_tile_counts{
        0, 10, 20, 60, 160, 430, 1140, 3010,
    };

    check_counts_and_area(aper::Tiling::p2, p2_triangle_counts, p2_tile_counts);
    check_counts_and_area(aper::Tiling::p3, p3_triangle_counts, p3_tile_counts);

    CHECK(aper::predicted_triangle_count(aper::Tiling::p2, 12) == 1213930);
    CHECK(aper::predicted_triangle_count(aper::Tiling::p3, 12) == 750250);
}

void test_rhomb_geometry() {
    const auto triangles = aper::generate(aper::Tiling::p3, 7);
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
    const auto triangles = aper::generate(aper::Tiling::p2, 5);
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

void check_pdf(aper::Tiling tiling, std::string_view title) {
    const auto triangles = aper::generate(tiling, 3);
    const auto tiles = aper::pair_tiles(triangles, tiling);

    std::ostringstream first;
    std::ostringstream second;
    aper::write_pdf(first, tiles, tiling, 3);
    aper::write_pdf(second, tiles, tiling, 3);

    const auto pdf = first.str();
    CHECK(pdf == second.str());
    CHECK(pdf.starts_with("%PDF-1.4\n"));
    CHECK(pdf.ends_with("%%EOF\n"));
    CHECK(pdf.find(title) != std::string::npos);
    CHECK(pdf.find("/Creator (aper 0.2.0)") != std::string::npos);
    CHECK(pdf.find("/MediaBox [0 0 720 720]") != std::string::npos);
    CHECK(pdf.find("/Resources << >>") != std::string::npos);
    CHECK(pdf.find("/Subject (Sun seed at depth 3)") != std::string::npos);
    CHECK(pdf.find("1.0000 1.0000 1.0000 rg\n0 0 720.0000 720.0000 re f") !=
          std::string::npos);
    CHECK(pdf.find("0.0627 0.0863 0.1059 RG") != std::string::npos);
    CHECK(pdf.find("0.9059 0.8706 0.8039") != std::string::npos);
    CHECK(pdf.find("0.7843 0.4196 0.2902") != std::string::npos);
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
}

void test_pdf() {
    check_pdf(aper::Tiling::p2, "/Title (P2 Penrose tiling)");
    check_pdf(aper::Tiling::p3, "/Title (P3 Penrose tiling)");
}

} // namespace

int main() {
    test_counts_and_area();
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
