#pragma once

#include "system.hpp"

#include <filesystem>

namespace aper {

class RuleLibrary {
  public:
    void add_directory(const std::filesystem::path& directory);
    void add_default_directory();
    void add_fallback(const TilingCatalogue& catalogue);

    const TilingCatalogue& catalogue() const { return catalogue_; }

  private:
    TilingCatalogue catalogue_;
};

} // namespace aper
