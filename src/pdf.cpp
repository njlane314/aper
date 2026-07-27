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

struct Scheme {
    std::string_view name;
    Colour primary;
    Colour secondary;
};

constexpr Colour paper{1.0, 1.0, 1.0};
constexpr Colour ink{16.0 / 255.0, 22.0 / 255.0, 27.0 / 255.0};

[[nodiscard]] Scheme scheme(ColourScheme colour_scheme) {
    switch (colour_scheme) {
    case ColourScheme::flare:
        return {
            "flare",
            {49.0 / 255.0, 87.0 / 255.0, 213.0 / 255.0},
            {1.0, 99.0 / 255.0, 61.0 / 255.0},
        };
    case ColourScheme::grove:
        return {
            "grove",
            {0.0, 138.0 / 255.0, 90.0 / 255.0},
            {1.0, 193.0 / 255.0, 69.0 / 255.0},
        };
    case ColourScheme::electric:
        return {
            "electric",
            {113.0 / 255.0, 55.0 / 255.0, 200.0 / 255.0},
            {180.0 / 255.0, 214.0 / 255.0, 0.0},
        };
    case ColourScheme::tide:
        return {
            "tide",
            {216.0 / 255.0, 27.0 / 255.0, 96.0 / 255.0},
            {0.0, 167.0 / 255.0, 196.0 / 255.0},
        };
    }
    throw std::invalid_argument("unknown colour scheme");
}

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

[[nodiscard]] Colour blend(Colour first, Colour second, double amount) {
    return {
        first.red + amount * (second.red - first.red),
        first.green + amount * (second.green - first.green),
        first.blue + amount * (second.blue - first.blue),
    };
}

[[nodiscard]] std::string content_stream(std::span<const Tile> tiles,
                                         const Scheme& colours) {
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

    for (std::uint8_t fill = 0; fill <= maximum_fill; ++fill) {
        const auto used = std::any_of(tiles.begin(), tiles.end(), [fill](const Tile& tile) {
            return tile.fill == fill;
        });
        if (!used) {
            continue;
        }
        const auto amount =
            static_cast<double>(fill) / static_cast<double>(maximum_fill);
        set_colour(content, blend(colours.primary, colours.secondary, amount), "rg");
        for (const auto& tile : tiles) {
            if (tile.fill != fill) {
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
                                   Seed seed, ColourScheme colour_scheme,
                                   unsigned depth) {
    const auto colours = scheme(colour_scheme);
    const auto content = content_stream(tiles, colours);
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

    const auto family = tiling_name(tiling);
    const auto penrose =
        tiling == Tiling::p1 || tiling == Tiling::p2 || tiling == Tiling::p3;
    const auto subject =
        "<< /Title (" + std::string(family) +
        (penrose ? " Penrose tiling - " : " tiling - ") +
        std::string(seed_name(seed)) + ") /Creator (aper 0.6.0) /Subject (" +
        std::string(seed_name(seed)) + " seed at depth " + std::to_string(depth) +
        "; " + std::string(colours.name) + " colour scheme) >>";
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
               Seed seed, ColourScheme colour_scheme, unsigned depth) {
    if (tiles.empty()) {
        throw std::invalid_argument("cannot render an empty tiling");
    }
    for (const auto& tile : tiles) {
        if (tile.fill > maximum_fill) {
            throw std::invalid_argument("cannot render an unknown fill value");
        }
        if (tile.vertices.size() < 3) {
            throw std::invalid_argument(
                "cannot render a tile with fewer than three vertices");
        }

        double twice_area = 0.0;
        for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
            const auto vertex = tile.vertices[i];
            if (!std::isfinite(vertex.real()) || !std::isfinite(vertex.imag())) {
                throw std::invalid_argument("cannot render a non-finite tile");
            }
            const auto next = tile.vertices[(i + 1) % tile.vertices.size()];
            twice_area += vertex.real() * next.imag() - vertex.imag() * next.real();
        }
        if (!std::isfinite(twice_area) || twice_area == 0.0) {
            throw std::invalid_argument("cannot render a degenerate tile");
        }
    }

    const auto document = make_pdf(tiles, tiling, seed, colour_scheme, depth);
    output.write(document.data(), static_cast<std::streamsize>(document.size()));
}

} // namespace aper
