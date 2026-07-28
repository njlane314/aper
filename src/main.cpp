#include "pdf.hpp"
#include "system.hpp"
#include "view.hpp"

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr std::string_view version = "0.7.0";

struct Options {
    const aper::TilingSystem* system = nullptr;
    std::string seed;
    aper::ColourScheme colour_scheme = aper::default_colour_scheme;
    unsigned depth = 0;
    bool rule = false;
    bool seed_selected = false;
    bool depth_selected = false;
};

std::string system_names() {
    std::string names;
    const auto& systems = aper::tiling_catalogue().systems();
    for (std::size_t i = 0; i < systems.size(); ++i) {
        if (i != 0) {
            names += i + 1 == systems.size() ? ", or " : ", ";
        }
        names += systems[i].spec().id;
    }
    return names;
}

void help(std::ostream& output) {
    output << "usage: aper [-t type] [-s seed] [-c scheme] [-n depth] [-r]\n"
              "\n"
              "Draw a substitution-tiling patch or rule sheet as PDF.\n"
              "\n"
              "  -t, --tiling TYPE  choose a tiling system (default: p3)\n";
    for (const auto& system : aper::tiling_catalogue().systems()) {
        output << "                     " << system.spec().id << ": "
               << system.spec().name;
        if (!system.spec().aliases.empty()) {
            output << " (";
            for (std::size_t i = 0; i < system.spec().aliases.size(); ++i) {
                if (i != 0) {
                    output << ", ";
                }
                output << system.spec().aliases[i];
            }
            output << ')';
        }
        output << '\n';
    }

    output << "  -s, --seed NAME    choose the patch's starting design\n";
    for (const auto& system : aper::tiling_catalogue().systems()) {
        output << "                     " << system.spec().id << ": ";
        for (std::size_t i = 0; i < system.seeds().size(); ++i) {
            if (i != 0) {
                output << ", ";
            }
            output << system.seeds()[i].name;
        }
        output << " (default: " << system.spec().default_seed;
        for (const auto& seed : system.seeds()) {
            if (seed.minimum_depth > system.spec().depths.minimum) {
                output << "; " << seed.name << " requires depth " << seed.minimum_depth
                       << '+';
            }
        }
        output << ")\n";
    }

    output << "  -c, --colour NAME  flare, grove, electric, or tide\n"
              "                     (default: flare)\n"
              "  -n, --depth N      set patch substitution depth\n";
    for (const auto& system : aper::tiling_catalogue().systems()) {
        const auto depth = system.spec().depths;
        output << "                     " << system.spec().id << ": " << depth.minimum
               << '-' << depth.maximum << " (default: " << depth.recommended << ")\n";
    }
    output << "  -r, --rule         draw every prototile substitution rule\n"
              "                     (seed and depth are patch-only)\n"
              "  -h, --help         show this help\n"
              "  -V, --version      show the version\n";
}

aper::ColourScheme parse_colour_scheme(std::string_view text) {
    if (text == "flare") {
        return aper::ColourScheme::flare;
    }
    if (text == "grove") {
        return aper::ColourScheme::grove;
    }
    if (text == "electric") {
        return aper::ColourScheme::electric;
    }
    if (text == "tide") {
        return aper::ColourScheme::tide;
    }
    throw std::invalid_argument(
        "colour scheme must be flare, grove, electric, or tide");
}

const aper::TilingSystem& parse_tiling(std::string_view text) {
    const auto* system = aper::tiling_catalogue().find(text);
    if (system == nullptr) {
        throw std::invalid_argument("tiling must be " + system_names());
    }
    return *system;
}

unsigned parse_depth(std::string_view text) {
    unsigned depth = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), depth);
    if (error != std::errc{} || end != text.data() + text.size() || depth == 0) {
        throw std::invalid_argument("depth must be a positive integer");
    }
    return depth;
}

Options parse_options(int argc, char** argv) {
    Options options;
    options.system = &aper::tiling_catalogue().get("p3");

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];

        if (argument == "-h" || argument == "--help") {
            help(std::cout);
            std::exit(0);
        }
        if (argument == "-V" || argument == "--version") {
            std::cout << "aper " << version << '\n';
            std::exit(0);
        }
        if (argument == "-r" || argument == "--rule") {
            options.rule = true;
            continue;
        }
        if (argument == "-s" || argument == "--seed") {
            if (++i == argc) {
                throw std::invalid_argument(argument == "-s"
                                                ? "option -s requires a seed"
                                                : "option --seed requires a seed");
            }
            options.seed = argv[i];
            options.seed_selected = true;
            continue;
        }
        if (argument.starts_with("--seed=")) {
            options.seed = argument.substr(7);
            options.seed_selected = true;
            continue;
        }
        if (argument == "-c" || argument == "--colour") {
            if (++i == argc) {
                throw std::invalid_argument(
                    argument == "-c" ? "option -c requires a colour scheme"
                                     : "option --colour requires a colour scheme");
            }
            options.colour_scheme = parse_colour_scheme(argv[i]);
            continue;
        }
        if (argument.starts_with("--colour=")) {
            options.colour_scheme = parse_colour_scheme(argument.substr(9));
            continue;
        }
        if (argument == "-t" || argument == "--tiling") {
            if (++i == argc) {
                throw std::invalid_argument(argument == "-t"
                                                ? "option -t requires a tiling"
                                                : "option --tiling requires a tiling");
            }
            options.system = &parse_tiling(argv[i]);
            continue;
        }
        if (argument.starts_with("--tiling=")) {
            options.system = &parse_tiling(argument.substr(9));
            continue;
        }
        if (argument == "-n" || argument == "--depth") {
            if (++i == argc) {
                throw std::invalid_argument(argument == "-n"
                                                ? "option -n requires a depth"
                                                : "option --depth requires a depth");
            }
            options.depth = parse_depth(argv[i]);
            options.depth_selected = true;
            continue;
        }
        if (argument.starts_with("--depth=")) {
            options.depth = parse_depth(argument.substr(8));
            options.depth_selected = true;
            continue;
        }
        if (argument == "--") {
            if (i + 1 != argc) {
                throw std::invalid_argument("aper takes no operands");
            }
            break;
        }
        if (argument.starts_with('-')) {
            throw std::invalid_argument("unknown option: " + std::string(argument));
        }
        throw std::invalid_argument("aper takes no operands");
    }

    if (options.seed_selected && options.seed.empty()) {
        throw std::invalid_argument("seed must not be empty");
    }
    if (options.rule) {
        if (options.seed_selected || options.depth_selected) {
            throw std::invalid_argument(
                "seed and depth options are only available for patch output");
        }
        return options;
    }

    if (!options.seed_selected) {
        options.seed = options.system->spec().default_seed;
    }
    if (!options.depth_selected) {
        options.depth = options.system->spec().depths.recommended;
    }
    const auto* seed = options.system->find_seed(options.seed);
    if (seed == nullptr) {
        throw std::invalid_argument(options.seed + " seed is not available for " +
                                    options.system->spec().name);
    }
    const auto depths = options.system->spec().depths;
    if (options.depth < depths.minimum || options.depth > depths.maximum) {
        throw std::invalid_argument(
            options.system->spec().name + " depth must be an integer from " +
            std::to_string(depths.minimum) + " to " + std::to_string(depths.maximum));
    }
    if (options.depth < seed->minimum_depth) {
        throw std::invalid_argument(seed->name + " seed requires a depth from " +
                                    std::to_string(seed->minimum_depth) + " to " +
                                    std::to_string(depths.maximum));
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        const auto drawing =
            options.rule ? aper::RuleView{*options.system}.drawing()
                         : aper::PatchView{*options.system, options.seed, options.depth}
                               .drawing();
        aper::PdfRenderer{}.write(std::cout, drawing, options.colour_scheme);
        if (!std::cout) {
            throw std::runtime_error("could not write PDF to standard output");
        }
    } catch (const std::invalid_argument& error) {
        std::cerr << "aper: " << error.what()
                  << "\n"
                     "Try 'aper -h' for more information.\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "aper: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
