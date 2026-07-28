#pragma once

#include "system.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace aper {

enum class CandidateIssueKind {
    structural,
    non_simple_polygon,
    outside_parent,
    overlap,
    area_mismatch,
    edge_mismatch,
    numerical_ambiguity,
    resource_limit,
};

struct CandidateIssue {
    CandidateIssueKind kind = CandidateIssueKind::structural;
    unsigned generation = 0;
    std::optional<PrototileId> parent;
    std::string detail;
};

class CandidateReport {
  public:
    bool valid() const { return issues_.empty(); }
    std::span<const CandidateIssue> issues() const { return issues_; }

  private:
    friend class GeometricValidator;
    explicit CandidateReport(std::vector<CandidateIssue> issues);

    std::vector<CandidateIssue> issues_;
};

struct GeometricValidationOptions {
    double tolerance = 1.0e-9;
    unsigned generations = 1;
    std::size_t maximum_tiles = 2048;
};

class GeometricValidator {
  public:
    explicit GeometricValidator(GeometricValidationOptions options = {});
    CandidateReport validate(const TilingSystem& system) const;

  private:
    GeometricValidationOptions options_;
};

bool area_eigenvalue_matches(const TilingSystem& system, double tolerance = 1.0e-9);
bool incidence_is_primitive(const IncidenceMatrix& matrix);
std::string canonical_key(const TilingSystem& system, double tolerance = 1.0e-9);

class CandidateSource {
  public:
    using Visitor = std::function<bool(TilingSystem)>;

    virtual ~CandidateSource() = default;
    virtual void enumerate(const Visitor& visit) const = 0;
    virtual std::string canonicalise(const TilingSystem& candidate,
                                     double tolerance) const;
};

class SquareLatticeSearch final : public CandidateSource {
  public:
    void enumerate(const Visitor& visit) const override;
};

class BinarySquareSearch final : public CandidateSource {
  public:
    void enumerate(const Visitor& visit) const override;
    std::string canonicalise(const TilingSystem& candidate,
                             double tolerance) const override;
};

class PolyominoRepTileSearch final : public CandidateSource {
  public:
    explicit PolyominoRepTileSearch(unsigned cells = 3);

    unsigned cells() const { return cells_; }
    void enumerate(const Visitor& visit) const override;

  private:
    unsigned cells_;
};

struct DiscoveryOptions {
    GeometricValidationOptions geometry{1.0e-9, 5, 2048};
    std::size_t maximum_generated_candidates = 4096;
    std::size_t maximum_candidates = 64;
    bool require_primitive = true;
};

struct DiscoveredCandidate {
    TilingSystem system;
    std::string serialisation;
};

struct DiscoveryStatistics {
    std::size_t generated = 0;
    std::size_t structurally_valid = 0;
    std::size_t algebraically_valid = 0;
    std::size_t geometrically_valid = 0;
    std::size_t canonicalised = 0;
    std::size_t unique = 0;
};

struct DiscoveryResult {
    std::vector<DiscoveredCandidate> candidates;
    DiscoveryStatistics statistics;
};

class DiscoveryEngine {
  public:
    explicit DiscoveryEngine(DiscoveryOptions options = {});
    DiscoveryResult run(const CandidateSource& source) const;

  private:
    DiscoveryOptions options_;
};

} // namespace aper
