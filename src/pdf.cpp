#include "pdf.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <locale>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace aper {
namespace {

constexpr double page_size = 720.0;

struct Bounds {
    double minimum_x = std::numeric_limits<double>::max();
    double minimum_y = std::numeric_limits<double>::max();
    double maximum_x = std::numeric_limits<double>::lowest();
    double maximum_y = std::numeric_limits<double>::lowest();
};

struct Colour {
    double red;
    double green;
    double blue;
};

constexpr Colour paper{1.0, 1.0, 1.0};
constexpr Colour ink{16.0 / 255.0, 22.0 / 255.0, 27.0 / 255.0};
constexpr Colour limestone{231.0 / 255.0, 222.0 / 255.0, 205.0 / 255.0};
constexpr Colour copper{200.0 / 255.0, 107.0 / 255.0, 74.0 / 255.0};

void include(Bounds& bounds, Point point) {
    bounds.minimum_x = std::min(bounds.minimum_x, point.real());
    bounds.minimum_y = std::min(bounds.minimum_y, point.imag());
    bounds.maximum_x = std::max(bounds.maximum_x, point.real());
    bounds.maximum_y = std::max(bounds.maximum_y, point.imag());
}

void set_colour(std::ostream& output, const Colour& colour,
                std::string_view operation) {
    output << colour.red << ' ' << colour.green << ' ' << colour.blue << ' '
           << operation << '\n';
}

void append_object(std::string& document, std::vector<std::size_t>& offsets,
                   std::size_t number, std::string_view body) {
    offsets[number] = document.size();
    document += std::to_string(number);
    document += " 0 obj\n";
    document.append(body);
    if (!body.ends_with('\n')) {
        document += '\n';
    }
    document += "endobj\n";
}

[[nodiscard]] bool limestone_fill(Shape shape) {
    return shape == Shape::kite || shape == Shape::thick_rhomb;
}

[[nodiscard]] std::string content_stream(std::span<const Tile> tiles) {
    Bounds bounds;
    double shortest_edge = std::numeric_limits<double>::max();
    for (const auto& tile : tiles) {
        for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
            const auto vertex = tile.vertices[i];
            include(bounds, vertex);
            shortest_edge = std::min(
                shortest_edge,
                std::abs(tile.vertices[(i + 1) % tile.vertices.size()] - vertex));
        }
    }

    const auto centre_x = (bounds.minimum_x + bounds.maximum_x) / 2.0;
    const auto centre_y = (bounds.minimum_y + bounds.maximum_y) / 2.0;
    const auto artwork_span = std::max(bounds.maximum_x - bounds.minimum_x,
                                       bounds.maximum_y - bounds.minimum_y);
    const auto margin = artwork_span * 0.06;
    const auto view_span = artwork_span + 2.0 * margin;
    const auto view_x = centre_x - view_span / 2.0;
    const auto view_y = centre_y - view_span / 2.0;
    const auto scale = page_size / view_span;
    const auto stroke_width =
        std::max(shortest_edge * scale * 0.04, page_size / 1800.0);

    const auto page_x = [=](Point point) { return (point.real() - view_x) * scale; };
    const auto page_y = [=](Point point) { return (point.imag() - view_y) * scale; };

    std::ostringstream content;
    content.imbue(std::locale::classic());
    content << std::fixed << std::setprecision(4) << "q\n";
    set_colour(content, paper, "rg");
    content << "0 0 " << page_size << ' ' << page_size << " re f\n";
    set_colour(content, ink, "RG");
    content << stroke_width << " w\n1 J\n1 j\n";

    for (const auto use_limestone : {true, false}) {
        set_colour(content, use_limestone ? limestone : copper, "rg");
        for (const auto& tile : tiles) {
            if (limestone_fill(tile.shape) != use_limestone) {
                continue;
            }
            content << page_x(tile.vertices[0]) << ' ' << page_y(tile.vertices[0])
                    << " m\n";
            for (std::size_t i = 1; i < tile.vertices.size(); ++i) {
                content << page_x(tile.vertices[i]) << ' ' << page_y(tile.vertices[i])
                        << " l\n";
            }
            content << "b\n";
        }
    }

    content << "Q\n";
    return content.str();
}

[[nodiscard]] std::string make_pdf(std::span<const Tile> tiles, Tiling tiling,
                                   unsigned depth) {
    const auto content = content_stream(tiles);
    std::vector<std::size_t> offsets(6);
    std::string document{"%PDF-1.4\n%\xE2\xE3\xCF\xD3\n"};

    append_object(document, offsets, 1,
                  "<< /Type /Catalog /Pages 2 0 R "
                  "/ViewerPreferences << /DisplayDocTitle true >> >>");
    append_object(document, offsets, 2, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    append_object(document, offsets, 3,
                  "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 720 720] "
                  "/Resources << >> /Contents 4 0 R >>");

    std::string stream_object = "<< /Length " + std::to_string(content.size()) +
                                " >>\nstream\n" + content + "endstream";
    append_object(document, offsets, 4, stream_object);

    const auto family = tiling == Tiling::p2 ? "P2" : "P3";
    const auto subject = "<< /Title (" + std::string(family) +
                         " Penrose tiling) /Creator (aper 0.2.0) "
                         "/Subject (Sun seed at depth " +
                         std::to_string(depth) + ") >>";
    append_object(document, offsets, 5, subject);

    const auto xref_offset = document.size();
    std::ostringstream trailer;
    trailer.imbue(std::locale::classic());
    trailer << "xref\n0 " << offsets.size() << "\n"
            << "0000000000 65535 f \n";
    for (std::size_t i = 1; i < offsets.size(); ++i) {
        trailer << std::setfill('0') << std::setw(10) << offsets[i] << " 00000 n \n";
    }
    trailer << "trailer\n<< /Size " << offsets.size() << " /Root 1 0 R /Info 5 0 R >>\n"
            << "startxref\n"
            << xref_offset << "\n%%EOF\n";
    document += trailer.str();
    return document;
}

} // namespace

void write_pdf(std::ostream& output, std::span<const Tile> tiles, Tiling tiling,
               unsigned depth) {
    if (tiles.empty()) {
        throw std::invalid_argument("cannot render an empty tiling");
    }

    const auto document = make_pdf(tiles, tiling, depth);
    output.write(document.data(), static_cast<std::streamsize>(document.size()));
}

} // namespace aper
