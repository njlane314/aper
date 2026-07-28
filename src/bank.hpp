#pragma once

#include "system.hpp"

#include <cstddef>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace aper {

SourceReference encyclopedia_reference(std::string record);

enum class KnownMatchKind {
    exact_rule,
};

struct KnownTilingMatch {
    KnownMatchKind kind = KnownMatchKind::exact_rule;
    const TilingSystem* system = nullptr;
};

class KnownTilingBank {
  public:
    explicit KnownTilingBank(const TilingCatalogue& catalogue,
                             double tolerance = 1.0e-9);
    KnownTilingBank(TilingCatalogue&&, double = 1.0e-9) = delete;

    std::span<const TilingSystem* const> systems() const { return systems_; }
    std::vector<KnownTilingMatch> classify(const TilingSystem& candidate) const;

  private:
    double tolerance_;
    std::vector<const TilingSystem*> systems_;
    std::map<std::string, std::vector<const TilingSystem*>> exact_rules_;
};

} // namespace aper
