#pragma once

#include "penrose.hpp"

#include <iosfwd>
#include <span>

namespace aper {

void write_pdf(std::ostream& output, std::span<const Rhomb> rhombs, unsigned depth);

} // namespace aper
