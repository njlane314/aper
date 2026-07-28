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

Scheme scheme(ColourScheme colour_scheme) {
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

Colour blend(Colour first, Colour second, double amount) {
    return {
        first.red + amount * (second.red - first.red),
        first.green + amount * (second.green - first.green),
        first.blue + amount * (second.blue - first.blue),
    };
}

Colour colour(Paint paint, const Scheme& colours) {
    switch (paint.role()) {
    case PaintRole::paper:
        return paper;
    case PaintRole::ink:
        return ink;
    case PaintRole::palette:
        return blend(colours.primary, colours.secondary,
                     static_cast<double>(paint.fill()) /
                         static_cast<double>(maximum_fill));
    }
    throw std::invalid_argument("unknown paint role");
}

std::string pdf_string(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const auto byte : text) {
        const auto value = static_cast<unsigned char>(byte);
        if (byte == '(' || byte == ')' || byte == '\\') {
            result.push_back('\\');
            result.push_back(byte);
        } else if (value < 32 || value >= 127) {
            result.push_back('\\');
            result.push_back(static_cast<char>('0' + ((value >> 6) & 7)));
            result.push_back(static_cast<char>('0' + ((value >> 3) & 7)));
            result.push_back(static_cast<char>('0' + (value & 7)));
        } else {
            result.push_back(byte);
        }
    }
    return result;
}

struct Page {
    double width;
    double height;
    double scale;
};

Page page_for(const Viewport& viewport) {
    if (viewport.aspect_ratio() >= 1.0) {
        return {page_size, page_size / viewport.aspect_ratio(),
                page_size / viewport.width()};
    }
    return {page_size * viewport.aspect_ratio(), page_size,
            page_size / viewport.height()};
}

double shortest_edge(const Drawing& drawing) {
    auto result = std::numeric_limits<double>::max();
    for (const auto& polygon : drawing.polygons()) {
        for (std::size_t i = 0; i < polygon.vertices.size(); ++i) {
            result = std::min(
                result, std::abs(polygon.vertices[(i + 1) % polygon.vertices.size()] -
                                 polygon.vertices[i]));
        }
    }
    return result;
}

std::string drawing_content_stream(const Drawing& drawing, const Scheme& colours,
                                   const Page& page) {
    const auto& viewport = drawing.viewport();
    const auto page_x = [&](Point point) {
        return (point.real() - viewport.x()) * page.scale;
    };
    const auto page_y = [&](Point point) {
        return (point.imag() - viewport.y()) * page.scale;
    };
    const auto edge = shortest_edge(drawing);
    const auto stroke_width =
        std::isfinite(edge) ? std::max(edge * page.scale * 0.04, page_size / 1800.0)
                            : page_size / 1800.0;

    std::ostringstream content;
    content.imbue(std::locale::classic());
    content << std::fixed << std::setprecision(4) << "q\n";
    set_colour(content, colour(drawing.background(), colours), "rg");
    content << "0 0 " << page.width << ' ' << page.height << " re f\n";
    content << "0 0 " << page.width << ' ' << page.height << " re W n\n";
    content << "1 J\n1 j\n";

    for (const auto& polygon : drawing.polygons()) {
        set_colour(content, colour(polygon.fill, colours), "rg");
        set_colour(content, colour(polygon.stroke, colours), "RG");
        content << stroke_width << " w\n";
        content << page_x(polygon.vertices[0]) << ' ' << page_y(polygon.vertices[0])
                << " m\n";
        for (std::size_t i = 1; i < polygon.vertices.size(); ++i) {
            content << page_x(polygon.vertices[i]) << ' ' << page_y(polygon.vertices[i])
                    << " l\n";
        }
        content << "b\n";
    }

    for (const auto& arrow : drawing.arrows()) {
        const auto arrow_colour = colour(arrow.paint, colours);
        const auto width =
            arrow.width > 0.0 ? arrow.width * page.scale : page_size / 900.0;
        const auto head_size =
            arrow.head_size > 0.0 ? arrow.head_size * page.scale : page_size / 80.0;
        const Point start{page_x(arrow.start), page_y(arrow.start)};
        const Point end{page_x(arrow.end), page_y(arrow.end)};
        const auto unit = (end - start) / std::abs(end - start);
        const auto normal = Point{-unit.imag(), unit.real()};
        const auto head_base = end - head_size * unit;
        const auto wing = head_size * 0.42;

        set_colour(content, arrow_colour, "RG");
        content << width << " w\n"
                << start.real() << ' ' << start.imag() << " m\n"
                << head_base.real() << ' ' << head_base.imag() << " l\nS\n";
        set_colour(content, arrow_colour, "rg");
        content << end.real() << ' ' << end.imag() << " m\n"
                << (head_base + wing * normal).real() << ' '
                << (head_base + wing * normal).imag() << " l\n"
                << (head_base - wing * normal).real() << ' '
                << (head_base - wing * normal).imag() << " l\nh f\n";
    }

    content << "Q\n";
    return content.str();
}

std::string make_drawing_pdf(const Drawing& drawing, ColourScheme colour_scheme) {
    const auto colours = scheme(colour_scheme);
    const auto page = page_for(drawing.viewport());
    const auto content = drawing_content_stream(drawing, colours, page);
    std::vector<std::size_t> offsets(6);
    std::string document{"%PDF-1.4\n%\xE2\xE3\xCF\xD3\n"};

    append_object(document, offsets, 1,
                  "<< /Type /Catalog /Pages 2 0 R "
                  "/ViewerPreferences << /DisplayDocTitle true >> >>");
    append_object(document, offsets, 2, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>");

    std::ostringstream page_object;
    page_object.imbue(std::locale::classic());
    page_object << std::fixed << std::setprecision(4)
                << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " << page.width << ' '
                << page.height << "] /Resources << >> /Contents 4 0 R >>";
    append_object(document, offsets, 3, page_object.str());

    const auto stream_object = "<< /Length " + std::to_string(content.size()) +
                               " >>\nstream\n" + content + "endstream";
    append_object(document, offsets, 4, stream_object);

    auto subject = drawing.metadata().subject;
    if (!subject.empty()) {
        subject += "; ";
    }
    subject += std::string(colours.name) + " colour scheme";
    const auto information = "<< /Title (" + pdf_string(drawing.metadata().title) +
                             ") /Creator (aper 0.7.0) /Subject (" +
                             pdf_string(subject) + ") >>";
    append_object(document, offsets, 5, information);

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

std::string content_stream(std::span<const Tile> tiles, const Scheme& colours) {
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
        const auto used =
            std::any_of(tiles.begin(), tiles.end(),
                        [fill](const Tile& tile) { return tile.fill == fill; });
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

std::string make_pdf(std::span<const Tile> tiles, Tiling tiling, Seed seed,
                     ColourScheme colour_scheme, unsigned depth) {
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
        (penrose ? " Penrose tiling - " : " tiling - ") + std::string(seed_name(seed)) +
        ") /Creator (aper 0.7.0) /Subject (" + std::string(seed_name(seed)) +
        " seed at depth " + std::to_string(depth) + "; " + std::string(colours.name) +
        " colour scheme) >>";
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

void PdfRenderer::write(std::ostream& output, const Drawing& drawing,
                        ColourScheme colour_scheme) const {
    if (drawing.empty()) {
        throw std::invalid_argument("cannot render an empty drawing");
    }
    const auto document = make_drawing_pdf(drawing, colour_scheme);
    output.write(document.data(), static_cast<std::streamsize>(document.size()));
}

void write_pdf(std::ostream& output, const Drawing& drawing,
               ColourScheme colour_scheme) {
    PdfRenderer{}.write(output, drawing, colour_scheme);
}

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
