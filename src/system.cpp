#include "system.hpp"

#include <algorithm>
#include <cmath>
#include <compare>
#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace aper {
namespace {

constexpr double quantisation = 1.0e10;
constexpr double validation_tolerance = 1.0e-8;

struct QuantisedPoint {
    std::int64_t x;
    std::int64_t y;

    auto operator<=>(const QuantisedPoint&) const = default;
};

struct PlacementKey {
    PrototileId prototile;
    std::vector<QuantisedPoint> vertices;

    auto operator<=>(const PlacementKey&) const = default;
};

struct KeyedPlacement {
    PlacementKey key;
    Placement placement;
};

QuantisedPoint quantise(Point point) {
    return {
        std::llround(point.real() * quantisation),
        std::llround(point.imag() * quantisation),
    };
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

std::vector<Point> transformed_boundary(const Prototile& prototile,
                                        const Similarity& pose) {
    std::vector<Point> vertices;
    vertices.reserve(prototile.boundary.size());
    for (const auto vertex : prototile.boundary) {
        vertices.push_back(pose.apply(vertex));
    }
    return vertices;
}

PlacementKey placement_key(const Placement& placement,
                           std::span<const Prototile> prototiles) {
    if (placement.prototile >= prototiles.size()) {
        throw std::invalid_argument("patch refers to an unknown prototile");
    }
    auto vertices =
        transformed_boundary(prototiles[placement.prototile], placement.pose);
    std::vector<QuantisedPoint> quantised;
    quantised.reserve(vertices.size());
    for (const auto vertex : vertices) {
        quantised.push_back(quantise(vertex));
    }
    std::sort(quantised.begin(), quantised.end());
    return {placement.prototile, std::move(quantised)};
}

Patch deduplicated(Patch patch, std::span<const Prototile> prototiles) {
    std::vector<KeyedPlacement> keyed;
    keyed.reserve(patch.size());
    for (const auto& placement : patch.placements()) {
        keyed.push_back({placement_key(placement, prototiles), placement});
    }
    std::sort(keyed.begin(), keyed.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.key < rhs.key; });

    std::vector<Placement> unique;
    unique.reserve(keyed.size());
    for (std::size_t i = 0; i < keyed.size(); ++i) {
        if (i == 0 || keyed[i].key != keyed[i - 1].key) {
            unique.push_back(std::move(keyed[i].placement));
        }
    }
    return Patch{std::move(unique)};
}

class IdentityProjector final : public PatchProjector {
  public:
    std::vector<Tile> project(const TilingSystem& system,
                              const Patch& patch) const override {
        return materialise(system, patch);
    }
};

bool finite(Point point) {
    return std::isfinite(point.real()) && std::isfinite(point.imag());
}

} // namespace

SystemSpec::SystemSpec(std::string id_value, std::string name_value,
                       std::vector<std::string> alias_values,
                       std::string default_seed_value, DepthRange depth_values,
                       std::vector<SourceReference> source_values)
    : id(std::move(id_value)), name(std::move(name_value)),
      aliases(std::move(alias_values)), default_seed(std::move(default_seed_value)),
      depths(depth_values), sources(std::move(source_values)) {}

Similarity::Similarity(Point translation, Point multiplier, bool reflected)
    : translation_(translation), multiplier_(multiplier), reflected_(reflected) {}

Point Similarity::apply(Point point) const {
    if (reflected_) {
        point = std::conj(point);
    }
    return translation_ + multiplier_ * point;
}

Similarity Similarity::then(const Similarity& inner) const {
    const auto inner_multiplier =
        reflected_ ? std::conj(inner.multiplier_) : inner.multiplier_;
    return {
        apply(inner.translation_),
        multiplier_ * inner_multiplier,
        reflected_ != inner.reflected_,
    };
}

Patch::Patch(std::vector<Placement> placements) : placements_(std::move(placements)) {}

void Patch::add(Placement placement) { placements_.push_back(std::move(placement)); }

Patch Patch::transformed(const Similarity& pose) const {
    std::vector<Placement> transformed;
    transformed.reserve(placements_.size());
    for (const auto& placement : placements_) {
        transformed.push_back({placement.prototile, pose.then(placement.pose)});
    }
    return Patch{std::move(transformed)};
}

SubstitutionRule::SubstitutionRule(double inflation, std::vector<RuleEntry> entries,
                                   bool deduplicate)
    : inflation_(inflation), entries_(std::move(entries)), deduplicate_(deduplicate) {}

const Patch& SubstitutionRule::replacement(PrototileId parent) const {
    const auto entry =
        std::find_if(entries_.begin(), entries_.end(),
                     [parent](const auto& value) { return value.parent == parent; });
    if (entry == entries_.end()) {
        throw std::invalid_argument("substitution rule has no case for prototile");
    }
    return entry->replacement;
}

Patch SubstitutionRule::apply(const Patch& patch,
                              std::span<const Prototile> prototiles) const {
    std::size_t count = 0;
    for (const auto& placement : patch.placements()) {
        count += replacement(placement.prototile).size();
    }

    std::vector<Placement> children;
    children.reserve(count);
    for (const auto& parent : patch.placements()) {
        for (const auto& child : replacement(parent.prototile).placements()) {
            if (child.prototile >= prototiles.size()) {
                throw std::invalid_argument(
                    "substitution rule refers to an unknown prototile");
            }
            children.push_back({child.prototile, parent.pose.then(child.pose)});
        }
    }

    Patch result{std::move(children)};
    return deduplicate_ ? deduplicated(std::move(result), prototiles) : result;
}

IncidenceMatrix SubstitutionRule::incidence_matrix(std::size_t prototile_count) const {
    IncidenceMatrix matrix(prototile_count, std::vector<std::size_t>(prototile_count));
    for (const auto& entry : entries_) {
        if (entry.parent >= prototile_count) {
            throw std::invalid_argument(
                "substitution rule refers to an unknown parent prototile");
        }
        for (const auto& child : entry.replacement.placements()) {
            if (child.prototile >= prototile_count) {
                throw std::invalid_argument(
                    "substitution rule refers to an unknown child prototile");
            }
            ++matrix[entry.parent][child.prototile];
        }
    }
    return matrix;
}

TilingSystem::TilingSystem(SystemSpec spec, std::vector<Prototile> prototiles,
                           SubstitutionRule rule, std::vector<SeedPatch> seeds,
                           std::shared_ptr<const PatchProjector> projector)
    : spec_(std::move(spec)), prototiles_(std::move(prototiles)),
      rule_(std::move(rule)), seeds_(std::move(seeds)),
      projector_(std::move(projector)) {
    validation_issues_ = collect_validation_issues();
}

const Prototile& TilingSystem::prototile(PrototileId id) const {
    if (id >= prototiles_.size() || prototiles_[id].id != id) {
        throw std::invalid_argument("unknown prototile");
    }
    return prototiles_[id];
}

const SeedPatch* TilingSystem::find_seed(std::string_view name) const {
    const auto found =
        std::find_if(seeds_.begin(), seeds_.end(),
                     [name](const auto& candidate) { return candidate.name == name; });
    return found == seeds_.end() ? nullptr : &*found;
}

const SeedPatch& TilingSystem::seed(std::string_view name) const {
    const auto* found = find_seed(name);
    if (found == nullptr) {
        throw std::invalid_argument(std::string(name) + " seed is not available for " +
                                    spec_.name);
    }
    return *found;
}

Patch TilingSystem::generate_raw(std::string_view seed_name, unsigned depth) const {
    ensure_valid();
    if (depth > spec_.depths.maximum) {
        throw std::invalid_argument(spec_.name +
                                    " subdivision depth exceeds the supported limit");
    }
    auto result = seed(seed_name).patch;
    for (unsigned generation = 0; generation < depth; ++generation) {
        result = rule_.apply(result, prototiles_);
    }
    return result;
}

std::vector<Tile> TilingSystem::generate(std::string_view seed_name,
                                         unsigned depth) const {
    ensure_valid();
    const auto& seed_patch = seed(seed_name);
    if (depth > 0 && depth < seed_patch.minimum_depth) {
        throw std::invalid_argument(seed_patch.name + " seed requires depth " +
                                    std::to_string(seed_patch.minimum_depth) + "+");
    }
    return projector_->project(*this, generate_raw(seed_name, depth));
}

void TilingSystem::ensure_valid() const {
    if (!validation_issues_.empty()) {
        throw std::logic_error("invalid tiling system " + spec_.id + ": " +
                               validation_issues_.front());
    }
}

std::vector<std::string> TilingSystem::collect_validation_issues() const {
    std::vector<std::string> issues;
    if (spec_.id.empty()) {
        issues.emplace_back("empty system id");
    }
    if (spec_.name.empty()) {
        issues.emplace_back("empty display name");
    }
    if (spec_.depths.minimum == 0 || spec_.depths.minimum > spec_.depths.recommended ||
        spec_.depths.recommended > spec_.depths.maximum) {
        issues.emplace_back("invalid depth range");
    }
    for (std::size_t i = 0; i < spec_.sources.size(); ++i) {
        const auto& source = spec_.sources[i];
        if (source.collection.empty() || source.record.empty() || source.url.empty() ||
            source.citation.empty() || source.licence_url.empty()) {
            issues.emplace_back("source reference is incomplete");
        }
        for (std::size_t j = 0; j < i; ++j) {
            const auto& previous = spec_.sources[j];
            if ((source.collection == previous.collection &&
                 source.record == previous.record) ||
                source.url == previous.url) {
                issues.emplace_back("duplicate source reference");
                break;
            }
        }
    }
    if (prototiles_.empty()) {
        issues.emplace_back("no prototiles");
    }
    if (!std::isfinite(rule_.inflation()) || rule_.inflation() <= 1.0) {
        issues.emplace_back("inflation must be finite and greater than one");
    }
    if (projector_ == nullptr) {
        issues.emplace_back("no patch projector");
    }

    for (std::size_t index = 0; index < prototiles_.size(); ++index) {
        const auto& prototile = prototiles_[index];
        if (prototile.id != index) {
            issues.emplace_back("prototile ids must match their storage index");
        }
        if (prototile.name.empty()) {
            issues.emplace_back("empty prototile name");
        }
        if (prototile.boundary.size() < 3) {
            issues.emplace_back("prototile boundary has fewer than three vertices");
        }
        if (prototile.fill > maximum_fill) {
            issues.emplace_back("prototile fill is outside the palette");
        }
        if (!std::all_of(prototile.boundary.begin(), prototile.boundary.end(),
                         finite)) {
            issues.emplace_back("prototile boundary is not finite");
        } else if (std::abs(signed_area(prototile.boundary)) <= validation_tolerance) {
            issues.emplace_back("prototile boundary is degenerate");
        }
    }

    std::vector<bool> rule_parents(prototiles_.size());
    for (const auto& entry : rule_.entries()) {
        if (entry.parent >= prototiles_.size() || rule_parents[entry.parent]) {
            issues.emplace_back("rule parents must be unique known prototiles");
            continue;
        }
        rule_parents[entry.parent] = true;
        if (entry.replacement.empty()) {
            issues.emplace_back("empty replacement patch");
            continue;
        }

        for (const auto& child : entry.replacement.placements()) {
            if (child.prototile >= prototiles_.size()) {
                issues.emplace_back("replacement refers to an unknown prototile");
                continue;
            }
            if (!finite(child.pose.translation()) || !finite(child.pose.multiplier()) ||
                std::abs(child.pose.multiplier()) <= validation_tolerance) {
                issues.emplace_back("replacement has an invalid similarity");
                continue;
            }
            const auto expected_scale = 1.0 / rule_.inflation();
            if (std::abs(std::abs(child.pose.multiplier()) - expected_scale) >
                validation_tolerance) {
                issues.emplace_back("replacement does not use the system contraction");
            }
        }
    }
    if (rule_.entries().size() != prototiles_.size() ||
        !std::all_of(rule_parents.begin(), rule_parents.end(),
                     [](bool present) { return present; })) {
        issues.emplace_back("rule does not define every prototile");
    }

    if (seeds_.empty()) {
        issues.emplace_back("no seed patches");
    }
    std::vector<std::string_view> seed_names;
    for (const auto& seed_patch : seeds_) {
        if (seed_patch.name.empty() || seed_patch.patch.empty()) {
            issues.emplace_back("seed must have a name and at least one placement");
        }
        if (seed_patch.minimum_depth < spec_.depths.minimum ||
            seed_patch.minimum_depth > spec_.depths.maximum) {
            issues.emplace_back("seed has an invalid minimum depth");
        }
        if (std::find(seed_names.begin(), seed_names.end(), seed_patch.name) !=
            seed_names.end()) {
            issues.emplace_back("duplicate seed name");
        }
        seed_names.push_back(seed_patch.name);
        for (const auto& placement : seed_patch.patch.placements()) {
            if (placement.prototile >= prototiles_.size()) {
                issues.emplace_back("seed refers to an unknown prototile");
            }
            if (!finite(placement.pose.translation()) ||
                !finite(placement.pose.multiplier()) ||
                std::abs(placement.pose.multiplier()) <= validation_tolerance) {
                issues.emplace_back("seed has an invalid similarity");
            }
        }
    }
    if (find_seed(spec_.default_seed) == nullptr) {
        issues.emplace_back("default seed is not present");
    }

    return issues;
}

void TilingCatalogue::add(TilingSystem system) {
    if (!system.validate().empty()) {
        throw std::invalid_argument("invalid tiling system " + system.spec().id + ": " +
                                    system.validate().front());
    }
    const auto& spec = system.spec();
    if (find(spec.id) != nullptr) {
        throw std::invalid_argument("duplicate tiling id or alias: " + spec.id);
    }
    for (std::size_t i = 0; i < spec.aliases.size(); ++i) {
        const auto& alias = spec.aliases[i];
        if (alias.empty() || find(alias) != nullptr || alias == spec.id) {
            throw std::invalid_argument("duplicate tiling id or alias: " + alias);
        }
        if (std::find(spec.aliases.begin(),
                      spec.aliases.begin() + static_cast<std::ptrdiff_t>(i),
                      alias) != spec.aliases.begin() + static_cast<std::ptrdiff_t>(i)) {
            throw std::invalid_argument("duplicate tiling id or alias: " + alias);
        }
    }
    systems_.push_back(std::move(system));
}

const TilingSystem* TilingCatalogue::find(std::string_view name) const {
    for (const auto& system : systems_) {
        if (system.spec().id == name ||
            std::find(system.spec().aliases.begin(), system.spec().aliases.end(),
                      name) != system.spec().aliases.end()) {
            return &system;
        }
    }
    return nullptr;
}

const TilingSystem& TilingCatalogue::get(std::string_view name) const {
    const auto* system = find(name);
    if (system == nullptr) {
        throw std::invalid_argument("unknown tiling: " + std::string(name));
    }
    return *system;
}

std::shared_ptr<const PatchProjector> identity_projector() {
    static const auto projector = std::make_shared<const IdentityProjector>();
    return projector;
}

std::vector<Tile> materialise(const TilingSystem& system, const Patch& patch) {
    std::vector<Tile> tiles;
    tiles.reserve(patch.size());
    for (const auto& placement : patch.placements()) {
        const auto& prototile = system.prototile(placement.prototile);
        auto vertices = transformed_boundary(prototile, placement.pose);
        canonicalise(vertices);
        tiles.push_back({prototile.shape, std::move(vertices), prototile.fill});
    }
    sort_tiles(tiles);
    return tiles;
}

const TilingCatalogue& tiling_catalogue() {
    static const auto catalogue = [] {
        TilingCatalogue result;
        for (auto& system : detail::make_penrose_systems()) {
            result.add(std::move(system));
        }
        for (auto& system : detail::make_straight_systems()) {
            result.add(std::move(system));
        }
        return result;
    }();
    return catalogue;
}

} // namespace aper
