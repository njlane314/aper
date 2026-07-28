#pragma once

#include "system.hpp"

#include <iosfwd>
#include <string_view>

namespace aper {

class SystemReader {
  public:
    TilingSystem read(std::istream& input,
                      std::string_view source_name = "<input>") const;
};

class SystemWriter {
  public:
    // Projectors are presentation adapters and are not part of the file format.
    // This writes the system's raw prototiles, substitutions, and seeds.
    void write(std::ostream& output, const TilingSystem& system) const;
};

} // namespace aper
