#pragma once

#include "penrose.hpp"
#include "view.hpp"

#include <iosfwd>
#include <span>

namespace aper {

enum class ColourScheme {
    flare,
    grove,
    electric,
    tide,
};

inline constexpr ColourScheme default_colour_scheme = ColourScheme::flare;

class PdfRenderer {
  public:
    void write(std::ostream& output, const Drawing& drawing,
               ColourScheme colour_scheme = default_colour_scheme) const;
};

void write_pdf(std::ostream& output, const Drawing& drawing,
               ColourScheme colour_scheme = default_colour_scheme);

void write_pdf(std::ostream& output, std::span<const Tile> tiles, Tiling tiling,
               Seed seed, ColourScheme colour_scheme, unsigned depth);

} // namespace aper
