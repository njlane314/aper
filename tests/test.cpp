#include "pdf.hpp"
#include "penrose.hpp"

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

[[nodiscard]] double area(const aper::HalfRhomb& half) {
    return std::abs(cross(half.base_a - half.apex, half.base_b - half.apex)) / 2.0;
}

[[nodiscard]] double area(const aper::Rhomb& rhomb) {
    double twice_area = 0.0;
    for (std::size_t i = 0; i < rhomb.vertices.size(); ++i) {
        twice_area +=
            cross(rhomb.vertices[i], rhomb.vertices[(i + 1) % rhomb.vertices.size()]);
    }
    return std::abs(twice_area) / 2.0;
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

void test_counts_and_area() {
    constexpr std::array<std::size_t, 8> half_counts{
        10, 20, 50, 130, 340, 890, 2330, 6100,
    };
    constexpr std::array<std::size_t, 8> rhomb_counts{
        0, 10, 20, 60, 160, 430, 1140, 3010,
    };

    auto halves = aper::sun_seed();
    const auto expected_area = 5.0 * std::sin(std::numbers::pi / 5.0);

    for (unsigned depth = 0; depth < half_counts.size(); ++depth) {
        CHECK(halves.size() == half_counts[depth]);
        CHECK(aper::predicted_half_count(depth) == half_counts[depth]);

        double total_area = 0.0;
        for (const auto& half : halves) {
            total_area += area(half);
        }
        CHECK(close(total_area, expected_area, 1.0e-8));

        const auto rhombs = aper::pair_rhombs(halves);
        CHECK(rhombs.size() == rhomb_counts[depth]);

        if (depth + 1 < half_counts.size()) {
            halves = aper::subdivide(halves);
        }
    }

    CHECK(aper::predicted_half_count(12) == 750250);
}

void test_rhomb_geometry() {
    const auto halves = aper::generate(7);
    const auto rhombs = aper::pair_rhombs(halves);
    CHECK(!rhombs.empty());

    for (const auto& rhomb : rhombs) {
        std::array<double, 4> sides{};
        for (std::size_t i = 0; i < rhomb.vertices.size(); ++i) {
            sides[i] = std::abs(rhomb.vertices[(i + 1) % rhomb.vertices.size()] -
                                rhomb.vertices[i]);
        }
        for (const auto side : sides) {
            CHECK(close(side, sides.front(), 1.0e-9));
        }
        CHECK(area(rhomb) > 0.0);
    }
}

void test_pdf() {
    const auto halves = aper::generate(3);
    const auto rhombs = aper::pair_rhombs(halves);

    std::ostringstream first;
    std::ostringstream second;
    aper::write_pdf(first, rhombs, 3);
    aper::write_pdf(second, rhombs, 3);

    const auto pdf = first.str();
    CHECK(pdf == second.str());
    CHECK(pdf.starts_with("%PDF-1.4\n"));
    CHECK(pdf.ends_with("%%EOF\n"));
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
    CHECK(occurrences(pdf, " m\n") == rhombs.size());

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

} // namespace

int main() {
    test_counts_and_area();
    test_rhomb_geometry();
    test_pdf();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "all tests passed\n";
    return EXIT_SUCCESS;
}
