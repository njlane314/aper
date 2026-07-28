#include "view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace aper {
namespace {

constexpr double patch_aspect_ratio = 4.0 / 3.0;
constexpr double crop_clearance = 0.92;
constexpr double quantisation = 1.0e9;

struct Bounds {
    double minimum_x = std::numeric_limits<double>::max();
    double minimum_y = std::numeric_limits<double>::max();
    double maximum_x = std::numeric_limits<double>::lowest();
    double maximum_y = std::numeric_limits<double>::lowest();

    double width() const { return maximum_x - minimum_x; }
    double height() const { return maximum_y - minimum_y; }
    Point centre() const {
        return {(minimum_x + maximum_x) / 2.0, (minimum_y + maximum_y) / 2.0};
    }
};

struct QuantisedPoint {
    std::int64_t x;
    std::int64_t y;

    auto operator<=>(const QuantisedPoint&) const = default;
};

struct Edge {
    QuantisedPoint first;
    QuantisedPoint second;
    Point a;
    Point b;
};

void include(Bounds& bounds, Point point) {
    bounds.minimum_x = std::min(bounds.minimum_x, point.real());
    bounds.minimum_y = std::min(bounds.minimum_y, point.imag());
    bounds.maximum_x = std::max(bounds.maximum_x, point.real());
    bounds.maximum_y = std::max(bounds.maximum_y, point.imag());
}

Bounds bounds(std::span<const Point> vertices) {
    Bounds result;
    for (const auto vertex : vertices) {
        include(result, vertex);
    }
    return result;
}

Bounds bounds(std::span<const Tile> tiles) {
    Bounds result;
    for (const auto& tile : tiles) {
        for (const auto vertex : tile.vertices) {
            include(result, vertex);
        }
    }
    return result;
}

QuantisedPoint quantise(Point point) {
    return {
        std::llround(point.real() * quantisation),
        std::llround(point.imag() * quantisation),
    };
}

Edge edge(Point a, Point b) {
    auto first = quantise(a);
    auto second = quantise(b);
    if (second < first) {
        std::swap(first, second);
        std::swap(a, b);
    }
    return {first, second, a, b};
}

struct EdgeHash {
    std::size_t operator()(const Edge& value) const {
        const auto mix = [](std::size_t seed, std::int64_t part) {
            const auto hash = std::hash<std::int64_t>{}(part);
            return seed ^ (hash + 0x9e3779b9U + (seed << 6U) + (seed >> 2U));
        };
        auto result = mix(0, value.first.x);
        result = mix(result, value.first.y);
        result = mix(result, value.second.x);
        return mix(result, value.second.y);
    }
};

struct SameEdge {
    bool operator()(const Edge& lhs, const Edge& rhs) const {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }
};

double distance_to_segment(Point point, Point a, Point b) {
    const auto direction = b - a;
    const auto length_squared = std::norm(direction);
    if (length_squared == 0.0) {
        return std::abs(point - a);
    }
    const auto offset = point - a;
    const auto projection = std::clamp(
        (offset.real() * direction.real() + offset.imag() * direction.imag()) /
            length_squared,
        0.0, 1.0);
    return std::abs(point - (a + projection * direction));
}

double cross(Point first, Point second) {
    return first.real() * second.imag() - first.imag() * second.real();
}

bool contains(std::span<const Point> polygon, Point point) {
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const auto a = polygon[i];
        const auto b = polygon[j];
        const auto edge_vector = b - a;
        const auto offset = point - a;
        if (std::abs(cross(edge_vector, offset)) < 1.0e-10 &&
            point.real() >= std::min(a.real(), b.real()) - 1.0e-10 &&
            point.real() <= std::max(a.real(), b.real()) + 1.0e-10 &&
            point.imag() >= std::min(a.imag(), b.imag()) - 1.0e-10 &&
            point.imag() <= std::max(a.imag(), b.imag()) + 1.0e-10) {
            return true;
        }
        const auto crosses = (a.imag() > point.imag()) != (b.imag() > point.imag());
        if (crosses) {
            const auto x = (b.real() - a.real()) * (point.imag() - a.imag()) /
                               (b.imag() - a.imag()) +
                           a.real();
            if (point.real() < x) {
                inside = !inside;
            }
        }
    }
    return inside;
}

Point polygon_centre(std::span<const Point> polygon) {
    double twice_area = 0.0;
    Point weighted{};
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto a = polygon[i];
        const auto b = polygon[(i + 1) % polygon.size()];
        const auto weight = cross(a, b);
        twice_area += weight;
        weighted += weight * (a + b);
    }
    if (std::abs(twice_area) > 1.0e-14) {
        return weighted / (3.0 * twice_area);
    }

    Point average{};
    for (const auto vertex : polygon) {
        average += vertex;
    }
    return average / static_cast<double>(polygon.size());
}

Point area_weighted_centre(std::span<const Tile> tiles) {
    Point weighted{};
    double area = 0.0;
    for (const auto& tile : tiles) {
        double twice_area = 0.0;
        for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
            twice_area +=
                cross(tile.vertices[i], tile.vertices[(i + 1) % tile.vertices.size()]);
        }
        const auto tile_area = std::abs(twice_area) / 2.0;
        weighted += tile_area * polygon_centre(tile.vertices);
        area += tile_area;
    }
    return weighted / area;
}

bool inside(std::span<const Tile> tiles, Point point) {
    return std::any_of(tiles.begin(), tiles.end(), [point](const auto& tile) {
        return contains(tile.vertices, point);
    });
}

double boundary_clearance(std::span<const Edge> edges, Point point) {
    auto clearance = std::numeric_limits<double>::max();
    for (const auto& boundary : edges) {
        clearance =
            std::min(clearance, distance_to_segment(point, boundary.a, boundary.b));
    }
    return clearance;
}

Point interior_centre(std::span<const Tile> tiles, std::span<const Edge> boundary,
                      Point preferred, double tolerance) {
    if (inside(tiles, preferred) &&
        boundary_clearance(boundary, preferred) > tolerance) {
        return preferred;
    }

    const auto weighted = area_weighted_centre(tiles);
    if (inside(tiles, weighted) && boundary_clearance(boundary, weighted) > tolerance) {
        return weighted;
    }

    const auto closest = std::min_element(
        tiles.begin(), tiles.end(), [weighted](const auto& lhs, const auto& rhs) {
            return std::abs(polygon_centre(lhs.vertices) - weighted) <
                   std::abs(polygon_centre(rhs.vertices) - weighted);
        });
    return polygon_centre(closest->vertices);
}

Viewport fallback_viewport(const Bounds& artwork, Point centre) {
    auto width = artwork.width() * 0.72;
    auto height = artwork.height() * 0.72;
    if (width / height > patch_aspect_ratio) {
        width = height * patch_aspect_ratio;
    } else {
        height = width / patch_aspect_ratio;
    }
    return {centre.real() - width / 2.0, centre.imag() - height / 2.0, width, height};
}

Viewport patch_viewport(std::span<const Tile> tiles) {
    const auto artwork = bounds(tiles);

    // A valid tiling edge occurs once on the boundary and twice in the
    // interior. Toggling it in a hash set extracts the boundary in linear time
    // without retaining and sorting every edge in a deep patch.
    std::unordered_set<Edge, EdgeHash, SameEdge> boundary_edges;
    for (const auto& tile : tiles) {
        for (std::size_t i = 0; i < tile.vertices.size(); ++i) {
            const auto candidate =
                edge(tile.vertices[i], tile.vertices[(i + 1) % tile.vertices.size()]);
            const auto found = boundary_edges.find(candidate);
            if (found == boundary_edges.end()) {
                boundary_edges.insert(candidate);
            } else {
                boundary_edges.erase(found);
            }
        }
    }
    std::vector<Edge> boundary;
    boundary.reserve(boundary_edges.size());
    boundary.insert(boundary.end(), boundary_edges.begin(), boundary_edges.end());

    const auto tolerance = std::max(artwork.width(), artwork.height()) * 1.0e-10;
    const auto centre = interior_centre(tiles, boundary, artwork.centre(), tolerance);
    auto radius = boundary_clearance(boundary, centre);

    const auto half_width =
        std::min(centre.real() - artwork.minimum_x, artwork.maximum_x - centre.real());
    const auto half_height =
        std::min(centre.imag() - artwork.minimum_y, artwork.maximum_y - centre.imag());
    radius = std::min(radius, 5.0 * half_width / 4.0);
    radius = std::min(radius, 5.0 * half_height / 3.0);

    if (!std::isfinite(radius) || radius <= 1.0e-10) {
        return fallback_viewport(artwork, centre);
    }

    radius *= crop_clearance;
    const auto width = 8.0 * radius / 5.0;
    const auto height = 6.0 * radius / 5.0;
    return {centre.real() - width / 2.0, centre.imag() - height / 2.0, width, height};
}

bool covered(const Viewport& viewport, std::span<const Tile> tiles) {
    constexpr unsigned columns = 12;
    constexpr unsigned rows = 9;
    constexpr auto sample_count = (columns + 1) * (rows + 1);
    std::array<bool, sample_count> samples{};
    auto remaining = sample_count;

    const auto first_sample = [](double value, double origin, double span,
                                 unsigned divisions) {
        const auto position = std::ceil((value - origin) / span * divisions);
        return static_cast<unsigned>(
            std::clamp(position, 0.0, static_cast<double>(divisions)));
    };
    const auto last_sample = [](double value, double origin, double span,
                                unsigned divisions) {
        const auto position = std::floor((value - origin) / span * divisions);
        return static_cast<unsigned>(
            std::clamp(position, 0.0, static_cast<double>(divisions)));
    };

    for (const auto& tile : tiles) {
        if (!viewport.intersects(tile.vertices)) {
            continue;
        }
        const auto tile_bounds = bounds(tile.vertices);
        const auto first_column = first_sample(tile_bounds.minimum_x, viewport.x(),
                                               viewport.width(), columns);
        const auto last_column =
            last_sample(tile_bounds.maximum_x, viewport.x(), viewport.width(), columns);
        const auto first_row =
            first_sample(tile_bounds.minimum_y, viewport.y(), viewport.height(), rows);
        const auto last_row =
            last_sample(tile_bounds.maximum_y, viewport.y(), viewport.height(), rows);

        for (auto row = first_row; row <= last_row; ++row) {
            for (auto column = first_column; column <= last_column; ++column) {
                const auto index = row * (columns + 1) + column;
                if (samples[index]) {
                    continue;
                }
                const auto point = Point{
                    viewport.x() + viewport.width() * column / columns,
                    viewport.y() + viewport.height() * row / rows,
                };
                if (contains(tile.vertices, point)) {
                    samples[index] = true;
                    --remaining;
                    if (remaining == 0) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

DrawingPolygon polygon(const Tile& tile) {
    return {tile.vertices, Paint::palette(tile.fill), Paint::ink()};
}

std::vector<Tile> inflated_replacement(const TilingSystem& system,
                                       const RuleEntry& entry) {
    const auto inflation = system.rule().inflation();
    return materialise(
        system, entry.replacement.transformed(Similarity{{}, Point{inflation, 0.0}}));
}

std::vector<Tile> parent_tile(const TilingSystem& system, PrototileId parent) {
    return materialise(system, Patch{{Placement{parent, {}}}});
}

Point transformed(Point point, const Bounds& source, Point target, double scale) {
    return target + scale * (point - source.centre());
}

void add_tiles(Drawing& drawing, const std::vector<Tile>& tiles, const Bounds& source,
               Point target, double scale) {
    for (const auto& tile : tiles) {
        std::vector<Point> vertices;
        vertices.reserve(tile.vertices.size());
        for (const auto vertex : tile.vertices) {
            vertices.push_back(transformed(vertex, source, target, scale));
        }
        drawing.add({std::move(vertices), Paint::palette(tile.fill), Paint::ink()});
    }
}

double positive_span(double span) { return std::max(span, 1.0e-12); }

void add_rule_case(Drawing& drawing, const TilingSystem& system, const RuleEntry& entry,
                   double x, double y, double width, double height) {
    const auto parent = parent_tile(system, entry.parent);
    const auto replacement = inflated_replacement(system, entry);
    const auto parent_bounds = bounds(parent);
    const auto replacement_bounds = bounds(replacement);

    const auto parent_zone_width = width * 0.22;
    const auto replacement_zone_width = width * 0.43;
    const auto available_height = height * 0.72;
    const auto scale = std::min({
        parent_zone_width / positive_span(parent_bounds.width()),
        replacement_zone_width / positive_span(replacement_bounds.width()),
        available_height / positive_span(parent_bounds.height()),
        available_height / positive_span(replacement_bounds.height()),
    });

    const auto centre_y = y + height / 2.0;
    const Point parent_target{x + width * 0.16, centre_y};
    const Point replacement_target{x + width * 0.72, centre_y};
    add_tiles(drawing, parent, parent_bounds, parent_target, scale);
    add_tiles(drawing, replacement, replacement_bounds, replacement_target, scale);

    const auto marker_size = std::min(width, height);
    drawing.add({
        {x + width * 0.34, centre_y},
        {x + width * 0.46, centre_y},
        Paint::ink(),
        marker_size * 0.006,
        marker_size * 0.035,
    });
}

} // namespace

Paint::Paint(PaintRole role, std::uint8_t fill) : role_(role), fill_(fill) {}

Paint Paint::paper() { return {PaintRole::paper, 0}; }

Paint Paint::ink() { return {PaintRole::ink, 0}; }

Paint Paint::palette(std::uint8_t fill) {
    if (fill > maximum_fill) {
        throw std::invalid_argument("paint fill is outside the palette");
    }
    return {PaintRole::palette, fill};
}

Viewport::Viewport(Point minimum, Point maximum)
    : minimum_(minimum), maximum_(maximum) {
    if (!std::isfinite(minimum.real()) || !std::isfinite(minimum.imag()) ||
        !std::isfinite(maximum.real()) || !std::isfinite(maximum.imag()) ||
        width() <= 0.0 || height() <= 0.0) {
        throw std::invalid_argument("viewport must be finite and non-empty");
    }
}

Viewport::Viewport(double x, double y, double width, double height)
    : Viewport(Point{x, y}, Point{x + width, y + height}) {}

bool Viewport::intersects(std::span<const Point> polygon) const {
    if (polygon.empty()) {
        return false;
    }
    const auto polygon_bounds = bounds(polygon);
    return polygon_bounds.maximum_x >= minimum_.real() &&
           polygon_bounds.minimum_x <= maximum_.real() &&
           polygon_bounds.maximum_y >= minimum_.imag() &&
           polygon_bounds.minimum_y <= maximum_.imag();
}

Drawing::Drawing(Viewport viewport, DrawingMetadata metadata)
    : viewport_(std::move(viewport)), metadata_(std::move(metadata)) {}

void Drawing::set_background(Paint paint) { background_ = paint; }

void Drawing::add(DrawingPolygon polygon) {
    if (polygon.vertices.size() < 3) {
        throw std::invalid_argument("drawing polygon has fewer than three vertices");
    }
    double twice_area = 0.0;
    for (std::size_t i = 0; i < polygon.vertices.size(); ++i) {
        const auto vertex = polygon.vertices[i];
        const auto next = polygon.vertices[(i + 1) % polygon.vertices.size()];
        if (!std::isfinite(vertex.real()) || !std::isfinite(vertex.imag())) {
            throw std::invalid_argument("drawing polygon is not finite");
        }
        twice_area += cross(vertex, next);
    }
    if (!std::isfinite(twice_area) || std::abs(twice_area) <= 1.0e-12) {
        throw std::invalid_argument("drawing polygon is degenerate");
    }
    polygons_.push_back(std::move(polygon));
}

void Drawing::add(DrawingArrow arrow) {
    if (!std::isfinite(arrow.start.real()) || !std::isfinite(arrow.start.imag()) ||
        !std::isfinite(arrow.end.real()) || !std::isfinite(arrow.end.imag()) ||
        arrow.start == arrow.end || !std::isfinite(arrow.width) || arrow.width < 0.0 ||
        !std::isfinite(arrow.head_size) || arrow.head_size < 0.0) {
        throw std::invalid_argument("drawing arrow is invalid");
    }
    arrows_.push_back(std::move(arrow));
}

PatchView::PatchView(const TilingSystem& system, std::string_view seed, unsigned depth)
    : system_(system), seed_(seed), depth_(depth) {}

Drawing PatchView::drawing() const {
    const auto& seed = system_.seed(seed_);
    if (depth_ < seed.minimum_depth) {
        throw std::invalid_argument(seed.name + " seed requires depth " +
                                    std::to_string(seed.minimum_depth) + "+");
    }
    const auto tiles = system_.generate(seed_, depth_);
    if (tiles.empty()) {
        throw std::invalid_argument("cannot draw an empty patch");
    }
    const auto support = materialise(system_, seed.patch);
    auto viewport = patch_viewport(support);
    if (!covered(viewport, tiles)) {
        viewport = patch_viewport(tiles);
    }

    Drawing result{
        std::move(viewport),
        {system_.spec().name + " tiling - " + seed_,
         seed_ + " seed at depth " + std::to_string(depth_) + "; clipped 4:3 patch"},
    };
    for (const auto& tile : tiles) {
        if (result.viewport().intersects(tile.vertices)) {
            result.add(polygon(tile));
        }
    }
    return result;
}

RuleView::RuleView(const TilingSystem& system) : system_(system) {}

Drawing RuleView::drawing() const {
    constexpr double width = 16.0;
    constexpr double height = 12.0;
    Drawing result{
        Viewport{0.0, 0.0, width, height},
        {system_.spec().name + " substitution rule",
         std::to_string(system_.rule().entries().size()) + " raw rule cases at scale " +
             std::to_string(system_.rule().inflation())},
    };

    const auto entries = system_.rule().entries();
    const auto columns = entries.size() > 3 ? std::size_t{2} : std::size_t{1};
    const auto rows = (entries.size() + columns - 1) / columns;
    const auto cell_width = width / static_cast<double>(columns);
    const auto cell_height = height / static_cast<double>(rows);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto column = i % columns;
        const auto row = i / columns;
        add_rule_case(result, system_, entries[i],
                      static_cast<double>(column) * cell_width,
                      height - static_cast<double>(row + 1) * cell_height, cell_width,
                      cell_height);
    }
    return result;
}

} // namespace aper
