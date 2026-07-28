#include "discovery.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace aper {
namespace {

using Polygon = std::vector<Point>;

long double cross(Point first, Point second) {
    return static_cast<long double>(first.real()) *
               static_cast<long double>(second.imag()) -
           static_cast<long double>(first.imag()) *
               static_cast<long double>(second.real());
}

long double dot(Point first, Point second) {
    return static_cast<long double>(first.real()) *
               static_cast<long double>(second.real()) +
           static_cast<long double>(first.imag()) *
               static_cast<long double>(second.imag());
}

long double signed_area(std::span<const Point> polygon) {
    long double twice_area = 0.0L;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        twice_area += cross(polygon[i], polygon[(i + 1) % polygon.size()]);
    }
    return twice_area / 2.0L;
}

long double area(std::span<const Point> polygon) {
    return std::abs(signed_area(polygon));
}

bool finite(Point point) {
    return std::isfinite(point.real()) && std::isfinite(point.imag());
}

void make_anticlockwise(Polygon& polygon) {
    if (signed_area(polygon) < 0.0L) {
        std::reverse(polygon.begin(), polygon.end());
    }
}

bool point_on_segment(Point point, Point first, Point second, double tolerance) {
    const auto edge = second - first;
    const auto length = std::abs(edge);
    if (length <= tolerance) {
        return std::abs(point - first) <= tolerance;
    }
    const auto offset = point - first;
    if (std::abs(cross(edge, offset)) > static_cast<long double>(tolerance * length)) {
        return false;
    }
    const auto projection = dot(offset, edge);
    const auto squared_length = dot(edge, edge);
    const auto margin = static_cast<long double>(tolerance * length);
    return projection >= -margin && projection <= squared_length + margin;
}

int sign(long double value, double tolerance) {
    if (value > tolerance) {
        return 1;
    }
    if (value < -tolerance) {
        return -1;
    }
    return 0;
}

bool segments_intersect(Point a, Point b, Point c, Point d, double tolerance) {
    const auto ab_c = cross(b - a, c - a);
    const auto ab_d = cross(b - a, d - a);
    const auto cd_a = cross(d - c, a - c);
    const auto cd_b = cross(d - c, b - c);
    const auto first = sign(ab_c, tolerance);
    const auto second = sign(ab_d, tolerance);
    const auto third = sign(cd_a, tolerance);
    const auto fourth = sign(cd_b, tolerance);
    if (first * second < 0 && third * fourth < 0) {
        return true;
    }
    return (first == 0 && point_on_segment(c, a, b, tolerance)) ||
           (second == 0 && point_on_segment(d, a, b, tolerance)) ||
           (third == 0 && point_on_segment(a, c, d, tolerance)) ||
           (fourth == 0 && point_on_segment(b, c, d, tolerance));
}

bool segments_cross_properly(Point a, Point b, Point c, Point d, double tolerance) {
    const auto first = sign(cross(b - a, c - a), tolerance);
    const auto second = sign(cross(b - a, d - a), tolerance);
    const auto third = sign(cross(d - c, a - c), tolerance);
    const auto fourth = sign(cross(d - c, b - c), tolerance);
    return first * second < 0 && third * fourth < 0;
}

bool simple_polygon(std::span<const Point> polygon, double tolerance) {
    if (polygon.size() < 3 || !std::all_of(polygon.begin(), polygon.end(), finite) ||
        area(polygon) <= tolerance) {
        return false;
    }

    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto next = (i + 1) % polygon.size();
        if (std::abs(polygon[next] - polygon[i]) <= tolerance) {
            return false;
        }
        for (std::size_t j = i + 1; j < polygon.size(); ++j) {
            if (std::abs(polygon[i] - polygon[j]) <= tolerance) {
                return false;
            }
        }
        const auto after = (i + 2) % polygon.size();
        const auto incoming = polygon[next] - polygon[i];
        const auto outgoing = polygon[after] - polygon[next];
        if (std::abs(cross(incoming, outgoing)) <= tolerance &&
            dot(incoming, outgoing) < 0.0L) {
            return false;
        }
    }

    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto i_next = (i + 1) % polygon.size();
        for (std::size_t j = i + 1; j < polygon.size(); ++j) {
            const auto j_next = (j + 1) % polygon.size();
            if (i == j || i_next == j || j_next == i) {
                continue;
            }
            if (segments_intersect(polygon[i], polygon[i_next], polygon[j],
                                   polygon[j_next], tolerance)) {
                return false;
            }
        }
    }
    return true;
}

enum class PointLocation {
    outside,
    boundary,
    inside,
};

PointLocation locate(Point point, std::span<const Point> polygon, double tolerance) {
    bool inside = false;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto first = polygon[i];
        const auto second = polygon[(i + 1) % polygon.size()];
        if (point_on_segment(point, first, second, tolerance)) {
            return PointLocation::boundary;
        }
        const auto crosses =
            (first.imag() > point.imag()) != (second.imag() > point.imag());
        if (!crosses) {
            continue;
        }
        const auto x = first.real() + (point.imag() - first.imag()) *
                                          (second.real() - first.real()) /
                                          (second.imag() - first.imag());
        if (x > point.real()) {
            inside = !inside;
        }
    }
    return inside ? PointLocation::inside : PointLocation::outside;
}

struct Bounds {
    double minimum_x = std::numeric_limits<double>::max();
    double minimum_y = std::numeric_limits<double>::max();
    double maximum_x = std::numeric_limits<double>::lowest();
    double maximum_y = std::numeric_limits<double>::lowest();
};

Bounds bounds(std::span<const Point> polygon) {
    Bounds result;
    for (const auto point : polygon) {
        result.minimum_x = std::min(result.minimum_x, point.real());
        result.minimum_y = std::min(result.minimum_y, point.imag());
        result.maximum_x = std::max(result.maximum_x, point.real());
        result.maximum_y = std::max(result.maximum_y, point.imag());
    }
    return result;
}

bool bounds_overlap(const Bounds& first, const Bounds& second, double tolerance) {
    return first.maximum_x >= second.minimum_x - tolerance &&
           second.maximum_x >= first.minimum_x - tolerance &&
           first.maximum_y >= second.minimum_y - tolerance &&
           second.maximum_y >= first.minimum_y - tolerance;
}

bool polygon_inside(std::span<const Point> child, std::span<const Point> parent,
                    double tolerance) {
    for (std::size_t i = 0; i < child.size(); ++i) {
        const auto first = child[i];
        const auto second = child[(i + 1) % child.size()];
        if (locate(first, parent, tolerance) == PointLocation::outside ||
            locate((first + second) / 2.0, parent, tolerance) ==
                PointLocation::outside) {
            return false;
        }
        for (std::size_t j = 0; j < parent.size(); ++j) {
            if (segments_cross_properly(first, second, parent[j],
                                        parent[(j + 1) % parent.size()], tolerance)) {
                return false;
            }
        }
    }
    return true;
}

bool interior_probe_overlaps(std::span<const Point> first,
                             std::span<const Point> second, double tolerance) {
    for (std::size_t i = 0; i < first.size(); ++i) {
        const auto a = first[i];
        const auto b = first[(i + 1) % first.size()];
        const auto edge = b - a;
        const auto length = std::abs(edge);
        if (length <= tolerance) {
            continue;
        }
        const auto normal = Point{-edge.imag(), edge.real()} / length;
        const auto distance =
            std::min(length * 0.01, std::max(tolerance * 16.0, length * 1.0e-7));
        const auto probe = (a + b) / 2.0 + normal * distance;
        if (locate(probe, first, tolerance) == PointLocation::inside &&
            locate(probe, second, tolerance) == PointLocation::inside) {
            return true;
        }
    }
    return false;
}

bool polygons_overlap(std::span<const Point> first, std::span<const Point> second,
                      double tolerance) {
    if (!bounds_overlap(bounds(first), bounds(second), tolerance)) {
        return false;
    }
    for (std::size_t i = 0; i < first.size(); ++i) {
        for (std::size_t j = 0; j < second.size(); ++j) {
            if (segments_cross_properly(first[i], first[(i + 1) % first.size()],
                                        second[j], second[(j + 1) % second.size()],
                                        tolerance)) {
                return true;
            }
        }
    }
    if (std::any_of(first.begin(), first.end(),
                    [&](Point point) {
                        return locate(point, second, tolerance) ==
                               PointLocation::inside;
                    }) ||
        std::any_of(second.begin(), second.end(), [&](Point point) {
            return locate(point, first, tolerance) == PointLocation::inside;
        })) {
        return true;
    }
    return interior_probe_overlaps(first, second, tolerance) ||
           interior_probe_overlaps(second, first, tolerance);
}

struct QuantisedPoint {
    std::int64_t x = 0;
    std::int64_t y = 0;

    auto operator<=>(const QuantisedPoint&) const = default;
};

struct EdgeKey {
    QuantisedPoint first;
    QuantisedPoint second;

    auto operator<=>(const EdgeKey&) const = default;
};

struct EdgeBalance {
    int parent_count = 0;
    int parent_direction = 0;
    int child_count = 0;
    int child_direction = 0;
};

std::int64_t quantise(double value, double tolerance) {
    const auto scaled = value / tolerance;
    const auto limit = static_cast<double>(std::numeric_limits<std::int64_t>::max());
    if (!std::isfinite(scaled) || std::abs(scaled) >= limit) {
        throw std::invalid_argument("coordinate is outside the canonical range");
    }
    return std::llround(scaled);
}

QuantisedPoint quantise(Point point, double tolerance) {
    return {quantise(point.real(), tolerance), quantise(point.imag(), tolerance)};
}

void add_atomic_edge(std::map<EdgeKey, EdgeBalance>& ledger, QuantisedPoint first,
                     QuantisedPoint second, bool parent) {
    if (first == second) {
        return;
    }
    const auto forward = first < second;
    const EdgeKey key = forward ? EdgeKey{first, second} : EdgeKey{second, first};
    auto& balance = ledger[key];
    const auto direction = forward ? 1 : -1;
    if (parent) {
        ++balance.parent_count;
        balance.parent_direction += direction;
    } else {
        ++balance.child_count;
        balance.child_direction += direction;
    }
}

void add_polygon_edges(std::map<EdgeKey, EdgeBalance>& ledger,
                       std::span<const Point> polygon,
                       std::span<const Point> split_points, bool parent,
                       double tolerance) {
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto first = polygon[i];
        const auto second = polygon[(i + 1) % polygon.size()];
        const auto edge = second - first;
        const auto squared_length = dot(edge, edge);
        std::vector<std::pair<long double, Point>> points{{0.0L, first},
                                                          {1.0L, second}};
        for (const auto point : split_points) {
            if (!point_on_segment(point, first, second, tolerance)) {
                continue;
            }
            const auto parameter = dot(point - first, edge) / squared_length;
            if (parameter > 0.0L && parameter < 1.0L) {
                points.emplace_back(parameter, point);
            }
        }
        std::sort(points.begin(), points.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.first < rhs.first;
        });
        std::vector<Point> unique;
        for (const auto& [parameter, point] : points) {
            (void)parameter;
            if (unique.empty() || std::abs(point - unique.back()) > tolerance) {
                unique.push_back(point);
            }
        }
        for (std::size_t j = 1; j < unique.size(); ++j) {
            add_atomic_edge(ledger, quantise(unique[j - 1], tolerance),
                            quantise(unique[j], tolerance), parent);
        }
    }
}

bool edges_match(std::span<const Point> parent, std::span<const Polygon> children,
                 double tolerance) {
    std::map<QuantisedPoint, Point> distinct_points;
    for (const auto point : parent) {
        distinct_points.try_emplace(quantise(point, tolerance), point);
    }
    for (const auto& child : children) {
        for (const auto point : child) {
            distinct_points.try_emplace(quantise(point, tolerance), point);
        }
    }
    std::vector<Point> split_points;
    split_points.reserve(distinct_points.size());
    for (const auto& [key, point] : distinct_points) {
        (void)key;
        split_points.push_back(point);
    }

    std::map<EdgeKey, EdgeBalance> ledger;
    add_polygon_edges(ledger, parent, split_points, true, tolerance);
    for (const auto& child : children) {
        add_polygon_edges(ledger, child, split_points, false, tolerance);
    }
    for (const auto& [edge, balance] : ledger) {
        (void)edge;
        if (balance.parent_count != 0) {
            if (balance.parent_count != 1 || balance.child_count != 1 ||
                balance.parent_direction != balance.child_direction) {
                return false;
            }
        } else if (balance.child_count != 2 || balance.child_direction != 0) {
            return false;
        }
    }
    return true;
}

struct CoverIssue {
    CandidateIssueKind kind;
    std::string detail;
};

std::optional<CoverIssue> inspect_cover(Polygon parent, std::vector<Polygon> children,
                                        double tolerance) {
    make_anticlockwise(parent);
    if (!simple_polygon(parent, tolerance)) {
        return CoverIssue{CandidateIssueKind::non_simple_polygon,
                          "parent polygon is not simple"};
    }
    for (std::size_t i = 0; i < children.size(); ++i) {
        make_anticlockwise(children[i]);
        if (!simple_polygon(children[i], tolerance)) {
            return CoverIssue{CandidateIssueKind::non_simple_polygon,
                              "child polygon " + std::to_string(i) + " is not simple"};
        }
        if (!polygon_inside(children[i], parent, tolerance)) {
            return CoverIssue{CandidateIssueKind::outside_parent,
                              "child polygon " + std::to_string(i) +
                                  " lies outside its parent"};
        }
    }

    for (std::size_t i = 0; i < children.size(); ++i) {
        for (std::size_t j = i + 1; j < children.size(); ++j) {
            if (polygons_overlap(children[i], children[j], tolerance)) {
                return CoverIssue{CandidateIssueKind::overlap,
                                  "child polygons " + std::to_string(i) + " and " +
                                      std::to_string(j) + " overlap"};
            }
        }
    }

    long double child_area = 0.0L;
    for (const auto& child : children) {
        child_area += area(child);
    }
    const auto parent_area = area(parent);
    const auto area_tolerance =
        static_cast<long double>(tolerance) *
            std::max<long double>({1.0L, parent_area, child_area}) +
        std::numeric_limits<long double>::epsilon() *
            static_cast<long double>(children.size() * 8);
    if (std::abs(child_area - parent_area) > area_tolerance) {
        return CoverIssue{CandidateIssueKind::area_mismatch,
                          "child areas do not sum to the parent area"};
    }
    if (!edges_match(parent, children, tolerance)) {
        return CoverIssue{CandidateIssueKind::edge_mismatch,
                          "child edges do not form the parent boundary"};
    }
    return std::nullopt;
}

double polygon_diameter(std::span<const Point> polygon) {
    double diameter = 0.0;
    for (const auto first : polygon) {
        for (const auto second : polygon) {
            diameter = std::max(diameter, std::abs(second - first));
        }
    }
    return diameter;
}

Polygon normalised(std::span<const Point> polygon, Point origin, double scale) {
    Polygon result;
    result.reserve(polygon.size());
    for (const auto point : polygon) {
        result.push_back((point - origin) / scale);
    }
    return result;
}

Polygon transformed_boundary(const Prototile& prototile, const Similarity& pose) {
    Polygon result;
    result.reserve(prototile.boundary.size());
    for (const auto point : prototile.boundary) {
        result.push_back(pose.apply(point));
    }
    return result;
}

struct CanonicalFrame {
    Point origin;
    Point axis;
    bool reflected = false;

    Point apply(Point point) const {
        auto result = (point - origin) / axis;
        return reflected ? std::conj(result) : result;
    }
};

Polygon remove_collinear_vertices(std::span<const Point> polygon, double tolerance) {
    Polygon result{polygon.begin(), polygon.end()};
    bool changed = true;
    while (changed && result.size() > 3) {
        changed = false;
        for (std::size_t i = 0; i < result.size(); ++i) {
            const auto incoming =
                result[i] - result[(i + result.size() - 1) % result.size()];
            const auto outgoing = result[(i + 1) % result.size()] - result[i];
            const auto scale = std::abs(incoming) * std::abs(outgoing);
            if (std::abs(cross(incoming, outgoing)) <= tolerance * scale &&
                dot(incoming, outgoing) >= 0.0L) {
                result.erase(result.begin() + static_cast<std::ptrdiff_t>(i));
                changed = true;
                break;
            }
        }
    }
    return result;
}

std::vector<QuantisedPoint> canonical_polygon(std::span<const Point> polygon,
                                              const CanonicalFrame& frame,
                                              double tolerance) {
    const auto simplified = remove_collinear_vertices(polygon, tolerance);
    std::vector<QuantisedPoint> points;
    points.reserve(simplified.size());
    for (const auto point : simplified) {
        points.push_back(quantise(frame.apply(point), tolerance));
    }

    bool changed = true;
    while (changed && points.size() > 3) {
        changed = false;
        for (std::size_t i = 0; i < points.size(); ++i) {
            const auto& previous = points[(i + points.size() - 1) % points.size()];
            const auto& current = points[i];
            const auto& next = points[(i + 1) % points.size()];
            const auto first_x = static_cast<long double>(current.x) -
                                 static_cast<long double>(previous.x);
            const auto first_y = static_cast<long double>(current.y) -
                                 static_cast<long double>(previous.y);
            const auto second_x =
                static_cast<long double>(next.x) - static_cast<long double>(current.x);
            const auto second_y =
                static_cast<long double>(next.y) - static_cast<long double>(current.y);
            const auto turn = first_x * second_y - first_y * second_x;
            const auto direction = first_x * second_x + first_y * second_y;
            if (turn == 0.0L && direction >= 0.0L) {
                points.erase(points.begin() + static_cast<std::ptrdiff_t>(i));
                changed = true;
                break;
            }
        }
    }

    std::vector<QuantisedPoint> best;
    for (const bool reversed : {false, true}) {
        for (std::size_t start = 0; start < points.size(); ++start) {
            std::vector<QuantisedPoint> candidate;
            candidate.reserve(points.size());
            for (std::size_t offset = 0; offset < points.size(); ++offset) {
                const auto index =
                    reversed ? (start + points.size() - offset) % points.size()
                             : (start + offset) % points.size();
                candidate.push_back(points[index]);
            }
            if (best.empty() || candidate < best) {
                best = std::move(candidate);
            }
        }
    }
    return best;
}

std::string serialise_polygon(std::span<const Point> polygon,
                              const CanonicalFrame& frame, double tolerance) {
    const auto points = canonical_polygon(polygon, frame, tolerance);
    std::ostringstream output;
    output << points.size() << '[';
    for (const auto& point : points) {
        output << point.x << ',' << point.y << ';';
    }
    output << ']';
    return output.str();
}

std::string rule_case_key(const TilingSystem& system, PrototileId parent,
                          std::span<const PrototileId> renamed,
                          const CanonicalFrame& frame, double tolerance) {
    std::vector<std::string> children;
    const auto& replacement = system.rule().replacement(parent);
    children.reserve(replacement.size());
    for (const auto& placement : replacement.placements()) {
        const auto polygon =
            transformed_boundary(system.prototile(placement.prototile), placement.pose);
        children.push_back(std::to_string(renamed[placement.prototile]) +
                           (placement.pose.reflected() ? "r:" : "p:") +
                           serialise_polygon(polygon, frame, tolerance));
    }
    std::sort(children.begin(), children.end());

    auto result =
        serialise_polygon(system.prototile(parent).boundary, frame, tolerance) + "{";
    for (const auto& child : children) {
        result += child + '|';
    }
    result += '}';
    return result;
}

constexpr std::array binary_square_positions{
    Point{0.0, 0.0},
    Point{0.5, 0.0},
    Point{0.0, 0.5},
    Point{0.5, 0.5},
};

constexpr std::array<std::array<unsigned, 4>, 8> square_symmetries{{
    {{0, 1, 2, 3}},
    {{1, 3, 0, 2}},
    {{3, 2, 1, 0}},
    {{2, 0, 3, 1}},
    {{1, 0, 3, 2}},
    {{2, 3, 0, 1}},
    {{0, 2, 1, 3}},
    {{3, 1, 2, 0}},
}};

Patch binary_square_patch(unsigned mask) {
    std::vector<Placement> placements;
    placements.reserve(binary_square_positions.size());
    for (std::size_t cell = 0; cell < binary_square_positions.size(); ++cell) {
        placements.push_back({static_cast<PrototileId>((mask >> cell) & 1U),
                              Similarity{binary_square_positions[cell], {0.5, 0.0}}});
    }
    return Patch{std::move(placements)};
}

std::string two_digits(unsigned value) {
    return (value < 10 ? "0" : "") + std::to_string(value);
}

TilingSystem binary_square_system(unsigned first, unsigned second) {
    const auto masks = two_digits(first) + '-' + two_digits(second);
    const std::vector<Point> square{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    return {
        {"binary-square-" + masks, "Binary square " + masks, {}, "a", {1, 4, 7}},
        {{0, "a", Shape::generic_polygon, square, 0},
         {1, "b", Shape::generic_polygon, square, maximum_fill}},
        SubstitutionRule{
            2.0, {{0, binary_square_patch(first)}, {1, binary_square_patch(second)}}},
        {{"a", Patch{{Placement{0, {}}}}, 1}, {"b", Patch{{Placement{1, {}}}}, 1}},
        identity_projector(),
    };
}

unsigned binary_square_mask(const TilingSystem& system, PrototileId parent,
                            double tolerance) {
    const auto& replacement = system.rule().replacement(parent);
    if (replacement.size() != binary_square_positions.size()) {
        throw std::invalid_argument(
            "binary-square rule must place four children per parent");
    }

    std::array<bool, 4> occupied{};
    unsigned mask = 0;
    for (const auto& placement : replacement.placements()) {
        if (placement.prototile > 1 || placement.pose.reflected() ||
            std::abs(placement.pose.multiplier() - Point{0.5, 0.0}) > tolerance) {
            throw std::invalid_argument(
                "binary-square rule contains a non-lattice placement");
        }
        const auto translation = placement.pose.translation();
        const auto x = std::llround(2.0 * translation.real());
        const auto y = std::llround(2.0 * translation.imag());
        if (x < 0 || x > 1 || y < 0 || y > 1 ||
            std::abs(translation.real() - 0.5 * static_cast<double>(x)) > tolerance ||
            std::abs(translation.imag() - 0.5 * static_cast<double>(y)) > tolerance) {
            throw std::invalid_argument(
                "binary-square rule contains a non-lattice placement");
        }
        const auto cell = static_cast<std::size_t>(2 * y + x);
        if (occupied[cell]) {
            throw std::invalid_argument(
                "binary-square rule places two children in one cell");
        }
        occupied[cell] = true;
        mask |= static_cast<unsigned>(placement.prototile) << cell;
    }
    return mask;
}

bool unit_square(std::span<const Point> boundary, double tolerance) {
    if (boundary.size() != 4 || !simple_polygon(boundary, tolerance)) {
        return false;
    }
    std::array<bool, 4> occupied{};
    for (const auto point : boundary) {
        const auto x = std::llround(point.real());
        const auto y = std::llround(point.imag());
        if (x < 0 || x > 1 || y < 0 || y > 1 ||
            std::abs(point.real() - static_cast<double>(x)) > tolerance ||
            std::abs(point.imag() - static_cast<double>(y)) > tolerance) {
            return false;
        }
        const auto corner = static_cast<std::size_t>(2 * y + x);
        if (occupied[corner]) {
            return false;
        }
        occupied[corner] = true;
    }
    return true;
}

unsigned transform_mask(unsigned mask, std::span<const unsigned, 4> permutation) {
    unsigned transformed = 0;
    for (std::size_t cell = 0; cell < permutation.size(); ++cell) {
        if ((mask & (1U << cell)) != 0) {
            transformed |= 1U << permutation[cell];
        }
    }
    return transformed;
}

struct ExactCoverRow {
    std::vector<std::size_t> columns;
};

class ExactCover {
  public:
    ExactCover(std::size_t column_count, std::vector<ExactCoverRow> rows)
        : column_count_(column_count), rows_(std::move(rows)),
          rows_by_column_(column_count) {
        for (std::size_t row = 0; row < rows_.size(); ++row) {
            for (const auto column : rows_[row].columns) {
                if (column >= column_count_) {
                    throw std::invalid_argument("exact-cover row is out of range");
                }
                rows_by_column_[column].push_back(row);
            }
        }
    }

    std::vector<std::vector<std::size_t>> solve(std::size_t maximum_solutions) const {
        std::vector<std::vector<std::size_t>> solutions;
        std::vector<bool> covered(column_count_);
        std::vector<std::size_t> selected;
        search(covered, selected, solutions, maximum_solutions);
        return solutions;
    }

  private:
    bool feasible(std::size_t row, const std::vector<bool>& covered) const {
        return std::none_of(rows_[row].columns.begin(), rows_[row].columns.end(),
                            [&](std::size_t column) { return covered[column]; });
    }

    void search(std::vector<bool>& covered, std::vector<std::size_t>& selected,
                std::vector<std::vector<std::size_t>>& solutions,
                std::size_t maximum_solutions) const {
        if (solutions.size() >= maximum_solutions) {
            return;
        }
        if (std::all_of(covered.begin(), covered.end(),
                        [](bool value) { return value; })) {
            solutions.push_back(selected);
            return;
        }

        std::size_t pivot = column_count_;
        std::size_t choices = std::numeric_limits<std::size_t>::max();
        for (std::size_t column = 0; column < column_count_; ++column) {
            if (covered[column]) {
                continue;
            }
            const auto count = static_cast<std::size_t>(std::count_if(
                rows_by_column_[column].begin(), rows_by_column_[column].end(),
                [&](std::size_t row) { return feasible(row, covered); }));
            if (count < choices) {
                choices = count;
                pivot = column;
            }
        }
        if (pivot == column_count_ || choices == 0) {
            return;
        }

        for (const auto row : rows_by_column_[pivot]) {
            if (!feasible(row, covered)) {
                continue;
            }
            for (const auto column : rows_[row].columns) {
                covered[column] = true;
            }
            selected.push_back(row);
            search(covered, selected, solutions, maximum_solutions);
            selected.pop_back();
            for (const auto column : rows_[row].columns) {
                covered[column] = false;
            }
            if (solutions.size() >= maximum_solutions) {
                return;
            }
        }
    }

    std::size_t column_count_;
    std::vector<ExactCoverRow> rows_;
    std::vector<std::vector<std::size_t>> rows_by_column_;
};

struct LatticeCell {
    int x = 0;
    int y = 0;

    auto operator<=>(const LatticeCell&) const = default;
};

using LatticeCells = std::vector<LatticeCell>;

struct LatticeSymmetry {
    unsigned quarter_turns = 0;
    bool reflected = false;
};

struct OrientedLatticeCells {
    LatticeCells cells;
    LatticeSymmetry symmetry;
    LatticeCell offset;
};

struct LatticePlacement {
    LatticeCells occupied;
    LatticeSymmetry symmetry;
    LatticeCell translation;
};

constexpr std::array lattice_symmetries{
    LatticeSymmetry{0, false}, LatticeSymmetry{1, false}, LatticeSymmetry{2, false},
    LatticeSymmetry{3, false}, LatticeSymmetry{0, true},  LatticeSymmetry{1, true},
    LatticeSymmetry{2, true},  LatticeSymmetry{3, true},
};

constexpr std::array lattice_neighbours{
    LatticeCell{1, 0},
    LatticeCell{-1, 0},
    LatticeCell{0, 1},
    LatticeCell{0, -1},
};

LatticeCell transformed_vertex(LatticeCell vertex, const LatticeSymmetry& symmetry) {
    if (symmetry.reflected) {
        vertex.y = -vertex.y;
    }
    for (unsigned turn = 0; turn < symmetry.quarter_turns; ++turn) {
        vertex = {-vertex.y, vertex.x};
    }
    return vertex;
}

OrientedLatticeCells oriented_cells(std::span<const LatticeCell> cells,
                                    const LatticeSymmetry& symmetry) {
    LatticeCells transformed;
    transformed.reserve(cells.size());
    for (const auto cell : cells) {
        const std::array corners{
            transformed_vertex(cell, symmetry),
            transformed_vertex({cell.x + 1, cell.y}, symmetry),
            transformed_vertex({cell.x, cell.y + 1}, symmetry),
            transformed_vertex({cell.x + 1, cell.y + 1}, symmetry),
        };
        const auto minimum_x = std::min_element(corners.begin(), corners.end(),
                                                [](const auto& lhs, const auto& rhs) {
                                                    return lhs.x < rhs.x;
                                                })
                                   ->x;
        const auto minimum_y = std::min_element(corners.begin(), corners.end(),
                                                [](const auto& lhs, const auto& rhs) {
                                                    return lhs.y < rhs.y;
                                                })
                                   ->y;
        transformed.push_back({minimum_x, minimum_y});
    }

    const auto minimum_x =
        std::min_element(transformed.begin(), transformed.end(),
                         [](const auto& lhs, const auto& rhs) { return lhs.x < rhs.x; })
            ->x;
    const auto minimum_y =
        std::min_element(transformed.begin(), transformed.end(),
                         [](const auto& lhs, const auto& rhs) { return lhs.y < rhs.y; })
            ->y;
    for (auto& cell : transformed) {
        cell.x -= minimum_x;
        cell.y -= minimum_y;
    }
    std::sort(transformed.begin(), transformed.end());
    return {std::move(transformed), symmetry, {-minimum_x, -minimum_y}};
}

LatticeCells canonical_cells(std::span<const LatticeCell> cells) {
    LatticeCells best;
    for (const auto& symmetry : lattice_symmetries) {
        auto candidate = oriented_cells(cells, symmetry).cells;
        if (best.empty() || candidate < best) {
            best = std::move(candidate);
        }
    }
    return best;
}

std::vector<LatticeCells> free_polyominoes(unsigned cell_count) {
    if (cell_count == 0) {
        throw std::invalid_argument("polyomino cell count must be positive");
    }
    std::set<LatticeCells> shapes{{LatticeCells{{0, 0}}}};
    for (unsigned size = 1; size < cell_count; ++size) {
        std::set<LatticeCells> next;
        for (const auto& shape : shapes) {
            for (const auto cell : shape) {
                for (const auto neighbour : lattice_neighbours) {
                    const LatticeCell added{cell.x + neighbour.x, cell.y + neighbour.y};
                    if (std::binary_search(shape.begin(), shape.end(), added)) {
                        continue;
                    }
                    auto expanded = shape;
                    expanded.push_back(added);
                    next.insert(canonical_cells(expanded));
                }
            }
        }
        shapes = std::move(next);
    }
    return {shapes.begin(), shapes.end()};
}

bool contains_cell(std::span<const LatticeCell> cells, LatticeCell cell) {
    return std::binary_search(cells.begin(), cells.end(), cell);
}

std::vector<Point> polyomino_boundary(std::span<const LatticeCell> cells) {
    std::map<LatticeCell, LatticeCell> edges;
    const auto add_edge = [&](LatticeCell first, LatticeCell second) {
        if (!edges.emplace(first, second).second) {
            throw std::invalid_argument("polyomino boundary touches itself");
        }
    };
    for (const auto cell : cells) {
        if (!contains_cell(cells, {cell.x, cell.y - 1})) {
            add_edge({cell.x, cell.y}, {cell.x + 1, cell.y});
        }
        if (!contains_cell(cells, {cell.x + 1, cell.y})) {
            add_edge({cell.x + 1, cell.y}, {cell.x + 1, cell.y + 1});
        }
        if (!contains_cell(cells, {cell.x, cell.y + 1})) {
            add_edge({cell.x + 1, cell.y + 1}, {cell.x, cell.y + 1});
        }
        if (!contains_cell(cells, {cell.x - 1, cell.y})) {
            add_edge({cell.x, cell.y + 1}, {cell.x, cell.y});
        }
    }
    if (edges.empty()) {
        throw std::invalid_argument("polyomino has no boundary");
    }

    const auto start = edges.begin()->first;
    auto current = start;
    std::vector<LatticeCell> outline;
    do {
        outline.push_back(current);
        const auto edge = edges.find(current);
        if (edge == edges.end()) {
            throw std::invalid_argument("polyomino boundary is not a cycle");
        }
        current = edge->second;
        edges.erase(edge);
    } while (current != start);
    if (!edges.empty()) {
        throw std::invalid_argument("polyomino has more than one boundary cycle");
    }

    bool changed = true;
    while (changed && outline.size() > 3) {
        changed = false;
        for (std::size_t i = 0; i < outline.size(); ++i) {
            const auto previous = outline[(i + outline.size() - 1) % outline.size()];
            const auto point = outline[i];
            const auto next = outline[(i + 1) % outline.size()];
            const auto incoming_x = point.x - previous.x;
            const auto incoming_y = point.y - previous.y;
            const auto outgoing_x = next.x - point.x;
            const auto outgoing_y = next.y - point.y;
            if (incoming_x * outgoing_y - incoming_y * outgoing_x == 0 &&
                incoming_x * outgoing_x + incoming_y * outgoing_y > 0) {
                outline.erase(outline.begin() + static_cast<std::ptrdiff_t>(i));
                changed = true;
                break;
            }
        }
    }

    std::vector<Point> boundary;
    boundary.reserve(outline.size());
    for (const auto vertex : outline) {
        boundary.emplace_back(static_cast<double>(vertex.x),
                              static_cast<double>(vertex.y));
    }
    return boundary;
}

LatticeCells inflated_cells(std::span<const LatticeCell> cells, int inflation) {
    LatticeCells result;
    result.reserve(cells.size() * static_cast<std::size_t>(inflation * inflation));
    for (const auto cell : cells) {
        for (int y = 0; y < inflation; ++y) {
            for (int x = 0; x < inflation; ++x) {
                result.push_back({inflation * cell.x + x, inflation * cell.y + y});
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<LatticePlacement>
polyomino_placements(std::span<const LatticeCell> shape,
                     std::span<const LatticeCell> target) {
    std::map<LatticeCells, OrientedLatticeCells> orientations;
    for (const auto& symmetry : lattice_symmetries) {
        auto oriented = oriented_cells(shape, symmetry);
        orientations.try_emplace(oriented.cells, std::move(oriented));
    }

    const auto target_maximum_x =
        std::max_element(target.begin(), target.end(),
                         [](const auto& lhs, const auto& rhs) { return lhs.x < rhs.x; })
            ->x;
    const auto target_maximum_y =
        std::max_element(target.begin(), target.end(),
                         [](const auto& lhs, const auto& rhs) { return lhs.y < rhs.y; })
            ->y;
    std::map<LatticeCells, LatticePlacement> distinct;
    for (const auto& [cells, orientation] : orientations) {
        const auto maximum_x = std::max_element(cells.begin(), cells.end(),
                                                [](const auto& lhs, const auto& rhs) {
                                                    return lhs.x < rhs.x;
                                                })
                                   ->x;
        const auto maximum_y = std::max_element(cells.begin(), cells.end(),
                                                [](const auto& lhs, const auto& rhs) {
                                                    return lhs.y < rhs.y;
                                                })
                                   ->y;
        for (int y = 0; y <= target_maximum_y - maximum_y; ++y) {
            for (int x = 0; x <= target_maximum_x - maximum_x; ++x) {
                LatticeCells occupied;
                occupied.reserve(cells.size());
                for (const auto cell : cells) {
                    occupied.push_back({cell.x + x, cell.y + y});
                }
                if (!std::includes(target.begin(), target.end(), occupied.begin(),
                                   occupied.end())) {
                    continue;
                }
                distinct.try_emplace(occupied,
                                     LatticePlacement{occupied,
                                                      orientation.symmetry,
                                                      {orientation.offset.x + x,
                                                       orientation.offset.y + y}});
            }
        }
    }

    std::vector<LatticePlacement> result;
    result.reserve(distinct.size());
    for (auto& [cells, placement] : distinct) {
        (void)cells;
        result.push_back(std::move(placement));
    }
    return result;
}

Point lattice_multiplier(const LatticeSymmetry& symmetry, double scale) {
    switch (symmetry.quarter_turns) {
    case 0:
        return {scale, 0.0};
    case 1:
        return {0.0, scale};
    case 2:
        return {-scale, 0.0};
    case 3:
        return {0.0, -scale};
    }
    throw std::invalid_argument("invalid lattice rotation");
}

bool straight_polyomino(std::span<const LatticeCell> cells) {
    return std::all_of(cells.begin(), cells.end(),
                       [&](const auto cell) { return cell.x == cells.front().x; }) ||
           std::all_of(cells.begin(), cells.end(),
                       [&](const auto cell) { return cell.y == cells.front().y; });
}

std::vector<TilingSystem> polyomino_rep_tiles(unsigned cell_count) {
    constexpr int inflation = 2;
    constexpr std::size_t maximum_solutions_per_shape = 4096;
    auto shapes = free_polyominoes(cell_count);
    std::stable_sort(shapes.begin(), shapes.end(),
                     [](const auto& lhs, const auto& rhs) {
                         return straight_polyomino(lhs) < straight_polyomino(rhs);
                     });

    std::vector<TilingSystem> systems;
    std::size_t shape_index = 0;
    for (const auto& shape : shapes) {
        const auto target = inflated_cells(shape, inflation);
        const auto placements = polyomino_placements(shape, target);
        std::map<LatticeCell, std::size_t> column_by_cell;
        for (std::size_t column = 0; column < target.size(); ++column) {
            column_by_cell.emplace(target[column], column);
        }
        std::vector<ExactCoverRow> rows;
        rows.reserve(placements.size());
        for (const auto& placement : placements) {
            ExactCoverRow row;
            row.columns.reserve(placement.occupied.size());
            for (const auto cell : placement.occupied) {
                row.columns.push_back(column_by_cell.at(cell));
            }
            rows.push_back(std::move(row));
        }
        const auto solutions = ExactCover{target.size(), std::move(rows)}.solve(
            maximum_solutions_per_shape + 1);
        if (solutions.size() > maximum_solutions_per_shape) {
            throw std::runtime_error("polyomino exact-cover solution limit exceeded");
        }

        using SolutionKey = std::vector<LatticeCells>;
        std::map<SolutionKey, std::vector<LatticePlacement>> distinct_solutions;
        for (const auto& solution : solutions) {
            std::vector<LatticePlacement> selected;
            selected.reserve(solution.size());
            for (const auto row : solution) {
                selected.push_back(placements[row]);
            }
            std::sort(selected.begin(), selected.end(),
                      [](const auto& lhs, const auto& rhs) {
                          return lhs.occupied < rhs.occupied;
                      });
            SolutionKey key;
            key.reserve(selected.size());
            for (const auto& placement : selected) {
                key.push_back(placement.occupied);
            }
            distinct_solutions.try_emplace(std::move(key), std::move(selected));
        }

        const auto straight = straight_polyomino(shape);
        auto stem = "tile";
        auto base_id = "polyomino-" + std::to_string(cell_count) + '-' +
                       two_digits(static_cast<unsigned>(shape_index));
        auto display = std::to_string(cell_count) + "-cell polyomino " +
                       std::to_string(shape_index) + " rep-tile";
        if (cell_count == 1) {
            stem = "monomino";
            base_id = "polyomino-monomino";
            display = "Monomino rep-tile";
        } else if (cell_count == 2) {
            stem = "domino";
            base_id = "polyomino-domino";
            display = "Domino rep-tile";
        } else if (cell_count == 3 && straight) {
            stem = "i";
            base_id = "polyomino-i";
            display = "I-triomino rep-tile";
        } else if (cell_count == 3) {
            stem = "chair";
            base_id = "polyomino-chair";
            display = "Chair (L-triomino) rep-tile";
        }
        std::size_t solution_index = 0;
        for (auto& [key, solution] : distinct_solutions) {
            (void)key;
            std::vector<Placement> children;
            children.reserve(solution.size());
            const auto scale = 1.0 / static_cast<double>(inflation);
            for (const auto& placement : solution) {
                children.push_back(
                    {0,
                     Similarity{{scale * static_cast<double>(placement.translation.x),
                                 scale * static_cast<double>(placement.translation.y)},
                                lattice_multiplier(placement.symmetry, scale),
                                placement.symmetry.reflected}});
            }
            const auto suffix =
                distinct_solutions.size() == 1
                    ? std::string{}
                    : '-' + two_digits(static_cast<unsigned>(solution_index));
            systems.emplace_back(
                SystemSpec{base_id + suffix, display, {}, stem, {1, 4, 7}},
                std::vector<Prototile>{
                    {0, stem, Shape::generic_polygon, polyomino_boundary(shape), 0}},
                SubstitutionRule{static_cast<double>(inflation),
                                 {{0, Patch{std::move(children)}}}},
                std::vector<SeedPatch>{{stem, Patch{{Placement{0, {}}}}, 1}},
                identity_projector());
            ++solution_index;
        }
        ++shape_index;
    }
    return systems;
}

} // namespace

CandidateReport::CandidateReport(std::vector<CandidateIssue> issues)
    : issues_(std::move(issues)) {}

GeometricValidator::GeometricValidator(GeometricValidationOptions options)
    : options_(options) {
    if (!std::isfinite(options_.tolerance) || options_.tolerance <= 0.0) {
        throw std::invalid_argument("geometric tolerance must be finite and positive");
    }
    if (options_.generations == 0) {
        throw std::invalid_argument("geometric validation needs a generation");
    }
    if (options_.maximum_tiles == 0) {
        throw std::invalid_argument("geometric tile limit must be positive");
    }
}

CandidateReport GeometricValidator::validate(const TilingSystem& system) const {
    std::vector<CandidateIssue> issues;
    for (const auto& detail : system.validate()) {
        issues.push_back({CandidateIssueKind::structural, 0, std::nullopt, detail});
    }
    if (!issues.empty()) {
        return CandidateReport{std::move(issues)};
    }

    for (const auto& prototile : system.prototiles()) {
        if (!simple_polygon(prototile.boundary, options_.tolerance)) {
            issues.push_back({CandidateIssueKind::non_simple_polygon, 0, prototile.id,
                              "prototile boundary is not simple"});
        }
    }
    if (!issues.empty()) {
        return CandidateReport{std::move(issues)};
    }

    for (const auto& prototile : system.prototiles()) {
        Patch patch{{Placement{prototile.id, {}}}};
        for (unsigned generation = 1; generation <= options_.generations;
             ++generation) {
            std::size_t next_size = 0;
            bool limit_exceeded = false;
            for (const auto& placement : patch.placements()) {
                const auto count =
                    system.rule().replacement(placement.prototile).size();
                if (count > options_.maximum_tiles -
                                std::min(next_size, options_.maximum_tiles)) {
                    limit_exceeded = true;
                    break;
                }
                next_size += count;
            }
            if (limit_exceeded || next_size > options_.maximum_tiles) {
                issues.push_back({CandidateIssueKind::resource_limit, generation,
                                  prototile.id,
                                  "generation exceeds the geometric tile limit"});
                break;
            }

            patch = system.rule().apply(patch, system.prototiles());
            const auto tiles = materialise(system, patch);
            const auto diameter = polygon_diameter(prototile.boundary);
            if (!std::isfinite(diameter) || diameter <= options_.tolerance) {
                issues.push_back({CandidateIssueKind::non_simple_polygon, generation,
                                  prototile.id, "parent polygon has no stable scale"});
                break;
            }
            const auto origin = prototile.boundary.front();
            auto parent = normalised(prototile.boundary, origin, diameter);
            std::vector<Polygon> children;
            children.reserve(tiles.size());
            for (const auto& tile : tiles) {
                children.push_back(normalised(tile.vertices, origin, diameter));
            }
            try {
                const auto issue = inspect_cover(std::move(parent), std::move(children),
                                                 options_.tolerance);
                if (issue.has_value()) {
                    issues.push_back({issue->kind, generation, prototile.id,
                                      std::move(issue->detail)});
                    break;
                }
            } catch (const std::invalid_argument& error) {
                issues.push_back({CandidateIssueKind::numerical_ambiguity, generation,
                                  prototile.id, error.what()});
                break;
            }
        }
    }
    return CandidateReport{std::move(issues)};
}

bool area_eigenvalue_matches(const TilingSystem& system, double tolerance) {
    if (!std::isfinite(tolerance) || tolerance <= 0.0) {
        throw std::invalid_argument("area tolerance must be finite and positive");
    }
    if (!system.validate().empty()) {
        return false;
    }
    const auto prototiles = system.prototiles();
    std::vector<long double> areas;
    areas.reserve(prototiles.size());
    for (const auto& prototile : prototiles) {
        areas.push_back(area(prototile.boundary));
    }
    const auto matrix = system.rule().incidence_matrix(prototiles.size());
    const auto inflation_area =
        static_cast<long double>(system.rule().inflation()) * system.rule().inflation();
    for (std::size_t parent = 0; parent < matrix.size(); ++parent) {
        long double children = 0.0L;
        for (std::size_t child = 0; child < matrix[parent].size(); ++child) {
            children += static_cast<long double>(matrix[parent][child]) * areas[child];
        }
        const auto expected = inflation_area * areas[parent];
        const auto scale = std::max(children, expected);
        if (std::abs(children - expected) >
            static_cast<long double>(tolerance) * scale) {
            return false;
        }
    }
    return true;
}

bool incidence_is_primitive(const IncidenceMatrix& matrix) {
    if (matrix.empty() ||
        std::any_of(matrix.begin(), matrix.end(),
                    [&](const auto& row) { return row.size() != matrix.size(); })) {
        return false;
    }
    const auto size = matrix.size();
    std::vector<std::vector<bool>> adjacency(size, std::vector<bool>(size));
    for (std::size_t row = 0; row < size; ++row) {
        for (std::size_t column = 0; column < size; ++column) {
            adjacency[row][column] = matrix[row][column] != 0;
        }
    }
    auto power = adjacency;
    const auto bound = (size - 1) * (size - 1) + 1;
    for (std::size_t exponent = 1; exponent <= bound; ++exponent) {
        if (std::all_of(power.begin(), power.end(), [](const auto& row) {
                return std::all_of(row.begin(), row.end(),
                                   [](bool value) { return value; });
            })) {
            return true;
        }
        std::vector<std::vector<bool>> next(size, std::vector<bool>(size));
        for (std::size_t row = 0; row < size; ++row) {
            for (std::size_t column = 0; column < size; ++column) {
                for (std::size_t middle = 0; middle < size; ++middle) {
                    next[row][column] =
                        next[row][column] ||
                        (power[row][middle] && adjacency[middle][column]);
                }
            }
        }
        power = std::move(next);
    }
    return false;
}

std::string canonical_key(const TilingSystem& system, double tolerance) {
    if (!std::isfinite(tolerance) || tolerance <= 0.0) {
        throw std::invalid_argument("canonical tolerance must be finite and positive");
    }
    if (!system.validate().empty()) {
        throw std::invalid_argument("cannot canonicalise an invalid tiling system");
    }
    const auto count = system.prototiles().size();
    std::vector<PrototileId> order(count);
    std::iota(order.begin(), order.end(), PrototileId{0});

    const auto serialise = [&](std::span<const PrototileId> type_order) {
        std::vector<PrototileId> renamed(count);
        for (PrototileId replacement = 0; replacement < count; ++replacement) {
            renamed[type_order[replacement]] = replacement;
        }

        const std::string prefix =
            "aper-candidate-v2|types=" + std::to_string(count) + "|inflation=" +
            std::to_string(quantise(system.rule().inflation(), tolerance)) +
            "|deduplicate=" + (system.rule().deduplicates() ? "1" : "0") + '|';
        const auto anchor = remove_collinear_vertices(
            system.prototile(type_order.front()).boundary, tolerance);
        std::string best;
        for (std::size_t i = 0; i < anchor.size(); ++i) {
            for (const bool reverse_axis : {false, true}) {
                const auto first =
                    reverse_axis ? anchor[(i + 1) % anchor.size()] : anchor[i];
                const auto second =
                    reverse_axis ? anchor[i] : anchor[(i + 1) % anchor.size()];
                const auto axis = second - first;
                if (std::abs(axis) <= tolerance) {
                    continue;
                }
                for (const bool reflected : {false, true}) {
                    const CanonicalFrame frame{first, axis, reflected};
                    auto candidate = prefix;
                    for (PrototileId replacement = 0; replacement < count;
                         ++replacement) {
                        candidate += "T" + std::to_string(replacement) + ':' +
                                     rule_case_key(system, type_order[replacement],
                                                   renamed, frame, tolerance) +
                                     '|';
                    }
                    if (best.empty() || candidate < best) {
                        best = std::move(candidate);
                    }
                }
            }
        }
        if (best.empty()) {
            throw std::invalid_argument("prototile has no nonzero canonical edge");
        }
        return best;
    };

    constexpr std::size_t maximum_permuted_types = 8;
    if (count > maximum_permuted_types) {
        return serialise(order);
    }

    std::string best;
    do {
        auto candidate = serialise(order);
        if (best.empty() || candidate < best) {
            best = std::move(candidate);
        }
    } while (std::next_permutation(order.begin(), order.end()));
    return best;
}

std::string CandidateSource::canonicalise(const TilingSystem& candidate,
                                          double tolerance) const {
    return canonical_key(candidate, tolerance);
}

void BinarySquareSearch::enumerate(const Visitor& visit) const {
    constexpr unsigned patterns = 1U << binary_square_positions.size();
    for (unsigned first = 0; first < patterns; ++first) {
        for (unsigned second = 0; second < patterns; ++second) {
            if (!visit(binary_square_system(first, second))) {
                return;
            }
        }
    }
}

PolyominoRepTileSearch::PolyominoRepTileSearch(unsigned cells) : cells_(cells) {
    if (cells_ == 0 || cells_ > 6) {
        throw std::invalid_argument("polyomino cell count must be from 1 to 6");
    }
}

void PolyominoRepTileSearch::enumerate(const Visitor& visit) const {
    for (auto& candidate : polyomino_rep_tiles(cells_)) {
        if (!visit(std::move(candidate))) {
            return;
        }
    }
}

std::string BinarySquareSearch::canonicalise(const TilingSystem& candidate,
                                             double tolerance) const {
    if (!std::isfinite(tolerance) || tolerance <= 0.0) {
        throw std::invalid_argument(
            "binary-square tolerance must be finite and positive");
    }
    if (!candidate.validate().empty() || candidate.prototiles().size() != 2 ||
        std::abs(candidate.rule().inflation() - 2.0) > tolerance ||
        !std::all_of(candidate.prototiles().begin(), candidate.prototiles().end(),
                     [&](const auto& prototile) {
                         return unit_square(prototile.boundary, tolerance);
                     })) {
        throw std::invalid_argument(
            "binary-square canonicalisation requires a valid two-type unit-square "
            "rule");
    }

    const auto first = binary_square_mask(candidate, 0, tolerance);
    const auto second = binary_square_mask(candidate, 1, tolerance);
    auto best = std::pair{std::numeric_limits<unsigned>::max(),
                          std::numeric_limits<unsigned>::max()};
    constexpr unsigned complement = 0x0fU;
    for (const auto& symmetry : square_symmetries) {
        const auto transformed_first = transform_mask(first, symmetry);
        const auto transformed_second = transform_mask(second, symmetry);
        best = std::min(best, std::pair{transformed_first, transformed_second});
        best = std::min(best, std::pair{complement ^ transformed_second,
                                        complement ^ transformed_first});
    }
    return "aper-binary-square-v1|a=" + std::to_string(best.first) +
           "|b=" + std::to_string(best.second);
}

void SquareLatticeSearch::enumerate(const Visitor& visit) const {
    constexpr unsigned inflation = 2;
    constexpr std::size_t side = inflation;
    const auto cell_count = side * side;
    std::vector<ExactCoverRow> rows;
    rows.reserve(cell_count);
    for (std::size_t y = 0; y < side; ++y) {
        for (std::size_t x = 0; x < side; ++x) {
            rows.push_back({{y * side + x}});
        }
    }
    const auto solutions = ExactCover{cell_count, std::move(rows)}.solve(2);

    for (const auto& solution : solutions) {
        std::vector<Placement> placements;
        placements.reserve(solution.size());
        for (const auto row : solution) {
            const auto x = row % side;
            const auto y = row / side;
            const auto scale = 1.0 / static_cast<double>(inflation);
            placements.push_back({0, Similarity{{static_cast<double>(x) * scale,
                                                 static_cast<double>(y) * scale},
                                                {scale, 0.0}}});
        }
        std::sort(placements.begin(), placements.end(),
                  [](const auto& lhs, const auto& rhs) {
                      return std::tuple{lhs.pose.translation().imag(),
                                        lhs.pose.translation().real()} <
                             std::tuple{rhs.pose.translation().imag(),
                                        rhs.pose.translation().real()};
                  });

        const auto dimension =
            std::to_string(inflation) + "x" + std::to_string(inflation);
        constexpr std::size_t maximum_patch_tiles = 100000;
        unsigned maximum_depth = 0;
        std::size_t patch_tiles = 1;
        while (maximum_depth < 7 && patch_tiles <= maximum_patch_tiles / cell_count) {
            patch_tiles *= cell_count;
            ++maximum_depth;
        }
        TilingSystem candidate{
            SystemSpec{"square-" + dimension,
                       "Square " + dimension + " control",
                       {},
                       "square",
                       {1, std::min(4U, maximum_depth), maximum_depth}},
            std::vector<Prototile>{{0,
                                    "square",
                                    Shape::generic_polygon,
                                    {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
                                    0}},
            SubstitutionRule{static_cast<double>(inflation),
                             {{0, Patch{std::move(placements)}}}},
            std::vector<SeedPatch>{{"square", Patch{{Placement{0, {}}}}, 1}},
            identity_projector()};
        if (!visit(std::move(candidate))) {
            break;
        }
    }
}

DiscoveryEngine::DiscoveryEngine(DiscoveryOptions options) : options_(options) {
    if (!std::isfinite(options_.geometry.tolerance) ||
        options_.geometry.tolerance <= 0.0) {
        throw std::invalid_argument("discovery tolerance must be finite and positive");
    }
    if (options_.geometry.generations == 0 || options_.geometry.maximum_tiles == 0 ||
        options_.maximum_generated_candidates == 0 ||
        options_.maximum_candidates == 0) {
        throw std::invalid_argument("discovery limits must be positive");
    }
}

DiscoveryResult DiscoveryEngine::run(const CandidateSource& source) const {
    DiscoveryResult result;
    std::set<std::string> seen;
    const GeometricValidator validator{options_.geometry};
    const auto may_continue = [&] {
        return result.statistics.generated < options_.maximum_generated_candidates &&
               result.candidates.size() < options_.maximum_candidates;
    };
    source.enumerate([&](TilingSystem candidate) {
        ++result.statistics.generated;
        if (!candidate.validate().empty()) {
            return may_continue();
        }
        ++result.statistics.structurally_valid;
        const auto matrix =
            candidate.rule().incidence_matrix(candidate.prototiles().size());
        if (!area_eigenvalue_matches(candidate, options_.geometry.tolerance) ||
            (options_.require_primitive && !incidence_is_primitive(matrix))) {
            return may_continue();
        }
        ++result.statistics.algebraically_valid;
        if (!validator.validate(candidate).valid()) {
            return may_continue();
        }
        ++result.statistics.geometrically_valid;
        std::string serialisation;
        try {
            serialisation = source.canonicalise(candidate, options_.geometry.tolerance);
        } catch (const std::invalid_argument&) {
            return may_continue();
        }
        ++result.statistics.canonicalised;
        if (!seen.insert(serialisation).second) {
            return may_continue();
        }
        ++result.statistics.unique;
        result.candidates.push_back({std::move(candidate), std::move(serialisation)});
        return may_continue();
    });
    return result;
}

} // namespace aper
