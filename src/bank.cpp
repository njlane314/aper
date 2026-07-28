#include "bank.hpp"
#include "discovery.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace aper {

SourceReference encyclopedia_reference(std::string record) {
    constexpr auto collection = "Tilings Encyclopedia";
    constexpr auto base = "https://tilings.math.uni-bielefeld.de/substitution/";
    constexpr auto citation =
        "D. Frettlöh, F. Gähler, E. Harriss: Tilings Encyclopedia";
    constexpr auto licence = "https://creativecommons.org/licenses/by-nc-sa/2.0/";
    return {collection, record, base + record + '/', citation, licence};
}

KnownTilingBank::KnownTilingBank(const TilingCatalogue& catalogue, double tolerance)
    : tolerance_(tolerance) {
    if (!std::isfinite(tolerance_) || tolerance_ <= 0.0) {
        throw std::invalid_argument(
            "known-tiling tolerance must be finite and positive");
    }
    for (const auto& system : catalogue.systems()) {
        if (system.spec().sources.empty()) {
            continue;
        }
        systems_.push_back(&system);
        exact_rules_[canonical_key(system, tolerance_)].push_back(&system);
    }
}

std::vector<KnownTilingMatch>
KnownTilingBank::classify(const TilingSystem& candidate) const {
    std::vector<KnownTilingMatch> matches;
    const auto found = exact_rules_.find(canonical_key(candidate, tolerance_));
    if (found == exact_rules_.end()) {
        return matches;
    }
    matches.reserve(found->second.size());
    for (const auto* system : found->second) {
        matches.push_back({KnownMatchKind::exact_rule, system});
    }
    return matches;
}

std::vector<KnownTilingMatch>
KnownTilingBank::classify(const TilingSystem& candidate,
                          const CandidateSource& source) const {
    const auto key = source.canonicalise(candidate, tolerance_);
    std::vector<KnownTilingMatch> matches;
    for (const auto* system : systems_) {
        try {
            if (source.canonicalise(*system, tolerance_) == key) {
                matches.push_back({KnownMatchKind::exact_rule, system});
            }
        } catch (const std::invalid_argument&) {
            // A source-specific equivalence need not accept unrelated systems.
        }
    }
    return matches;
}

} // namespace aper
