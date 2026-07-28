#pragma once

#include "geometry.hpp"

#include <cstddef>
#include <deque>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aper {

using PrototileId = std::size_t;

class Similarity {
  public:
    Similarity() = default;
    Similarity(Point translation, Point multiplier, bool reflected = false);

    Point apply(Point point) const;
    Similarity then(const Similarity& inner) const;

    Point translation() const { return translation_; }
    Point multiplier() const { return multiplier_; }
    bool reflected() const { return reflected_; }

  private:
    Point translation_{};
    Point multiplier_{1.0, 0.0};
    bool reflected_ = false;
};

struct Placement {
    PrototileId prototile = 0;
    Similarity pose{};
};

class Patch {
  public:
    Patch() = default;
    explicit Patch(std::vector<Placement> placements);

    std::span<const Placement> placements() const { return placements_; }
    bool empty() const { return placements_.empty(); }
    std::size_t size() const { return placements_.size(); }
    void add(Placement placement);
    Patch transformed(const Similarity& pose) const;

  private:
    std::vector<Placement> placements_;
};

struct Prototile {
    PrototileId id = 0;
    std::string name;
    Shape shape = Shape::generic_polygon;
    std::vector<Point> boundary;
    std::uint8_t fill = 0;
};

struct RuleEntry {
    PrototileId parent = 0;
    Patch replacement;
};

using IncidenceMatrix = std::vector<std::vector<std::size_t>>;

class SubstitutionRule {
  public:
    SubstitutionRule() = default;
    SubstitutionRule(double inflation, std::vector<RuleEntry> entries,
                     bool deduplicate = false);

    double inflation() const { return inflation_; }
    std::span<const RuleEntry> entries() const { return entries_; }
    bool deduplicates() const { return deduplicate_; }
    const Patch& replacement(PrototileId parent) const;
    Patch apply(const Patch& patch, std::span<const Prototile> prototiles) const;
    IncidenceMatrix incidence_matrix(std::size_t prototiles) const;

  private:
    double inflation_ = 1.0;
    std::vector<RuleEntry> entries_;
    bool deduplicate_ = false;
};

struct SeedPatch {
    std::string name;
    Patch patch;
    unsigned minimum_depth = 1;
};

struct DepthRange {
    unsigned minimum = 1;
    unsigned recommended = 1;
    unsigned maximum = 1;
};

struct SourceReference {
    std::string collection;
    std::string record;
    std::string url;
    std::string citation;
    std::string licence_url;
};

struct SystemSpec {
    SystemSpec() = default;
    SystemSpec(std::string id, std::string name, std::vector<std::string> aliases,
               std::string default_seed, DepthRange depths,
               std::vector<SourceReference> sources = {});

    std::string id;
    std::string name;
    std::vector<std::string> aliases;
    std::string default_seed;
    DepthRange depths;
    std::vector<SourceReference> sources;
};

class TilingSystem;

class PatchProjector {
  public:
    virtual ~PatchProjector() = default;
    virtual std::vector<Tile> project(const TilingSystem& system,
                                      const Patch& patch) const = 0;
};

class TilingSystem {
  public:
    TilingSystem(SystemSpec spec, std::vector<Prototile> prototiles,
                 SubstitutionRule rule, std::vector<SeedPatch> seeds,
                 std::shared_ptr<const PatchProjector> projector);

    const SystemSpec& spec() const { return spec_; }
    std::span<const Prototile> prototiles() const { return prototiles_; }
    const Prototile& prototile(PrototileId id) const;
    const SubstitutionRule& rule() const { return rule_; }
    std::span<const SeedPatch> seeds() const { return seeds_; }
    const SeedPatch* find_seed(std::string_view name) const;
    const SeedPatch& seed(std::string_view name) const;
    Patch generate_raw(std::string_view seed_name, unsigned depth) const;
    std::vector<Tile> generate(std::string_view seed_name, unsigned depth) const;
    std::span<const std::string> validate() const { return validation_issues_; }

  private:
    std::vector<std::string> collect_validation_issues() const;
    void ensure_valid() const;

    SystemSpec spec_;
    std::vector<Prototile> prototiles_;
    SubstitutionRule rule_;
    std::vector<SeedPatch> seeds_;
    std::shared_ptr<const PatchProjector> projector_;
    std::vector<std::string> validation_issues_;
};

class TilingCatalogue {
  public:
    void add(TilingSystem system);
    const std::deque<TilingSystem>& systems() const { return systems_; }
    const TilingSystem* find(std::string_view name) const;
    const TilingSystem& get(std::string_view name) const;

  private:
    // Discovery can append candidates without invalidating views or references
    // to systems that are already being inspected.
    std::deque<TilingSystem> systems_;
};

std::shared_ptr<const PatchProjector> identity_projector();
std::vector<Tile> materialise(const TilingSystem& system, const Patch& patch);
const TilingCatalogue& tiling_catalogue();

namespace detail {

std::vector<TilingSystem> make_penrose_systems();
std::vector<TilingSystem> make_straight_systems();

} // namespace detail

} // namespace aper
