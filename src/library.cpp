#include "library.hpp"

#include "definition.hpp"
#include "discovery.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace aper {

#ifndef APER_RULES_DIRECTORY
#define APER_RULES_DIRECTORY "/usr/local/share/aper/rules"
#endif

void RuleLibrary::add_directory(const std::filesystem::path& directory) {
    if (!std::filesystem::is_directory(directory)) {
        throw std::invalid_argument("rule-library directory does not exist: " +
                                    directory.string());
    }

    std::vector<std::filesystem::path> definitions;
    for (const auto& entry : std::filesystem::recursive_directory_iterator{directory}) {
        if (entry.is_regular_file() && entry.path().extension() == ".aper") {
            definitions.push_back(entry.path());
        }
    }
    std::sort(definitions.begin(), definitions.end());
    for (const auto& definition : definitions) {
        std::ifstream input{definition};
        if (!input) {
            throw std::invalid_argument("could not open rule definition: " +
                                        definition.string());
        }
        catalogue_.add(SystemReader{}.read(input, definition.string()));
    }
}

void RuleLibrary::add_default_directory() {
    const std::filesystem::path local{"rules"};
    if (std::filesystem::is_directory(local)) {
        add_directory(local);
        return;
    }

    const std::filesystem::path installed{APER_RULES_DIRECTORY};
    if (std::filesystem::is_directory(installed)) {
        add_directory(installed);
    }
}

void RuleLibrary::add_fallback(const TilingCatalogue& catalogue) {
    for (const auto& system : catalogue.systems()) {
        const auto* existing = catalogue_.find(system.spec().id);
        if (existing == nullptr) {
            catalogue_.add(system);
            continue;
        }
        if (existing->spec().id != system.spec().id) {
            throw std::invalid_argument("rule-library alias shadows system id: " +
                                        system.spec().id);
        }
        if (canonical_key(*existing) != canonical_key(system)) {
            throw std::invalid_argument(
                "rule-library definition conflicts with built-in: " + system.spec().id);
        }
    }
}

} // namespace aper
