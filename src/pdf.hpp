#pragma once

#include "penrose.hpp"

#include <iosfwd>
#include <span>

namespace aper {

void write_pdf(std::ostream& output, std::span<const Tile> tiles, Tiling tiling,
               unsigned depth);

} // namespace aper
