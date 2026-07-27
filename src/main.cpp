#include "pdf.hpp"
#include "penrose.hpp"

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr std::string_view version = "0.6.0";

struct Options {
    aper::Tiling tiling = aper::Tiling::p3;
    aper::Seed seed = aper::default_seed;
    aper::ColourScheme colour_scheme = aper::default_colour_scheme;
    unsigned depth = aper::default_depth;
    bool seed_selected = false;
    bool depth_selected = false;
};

void help(std::ostream& output) {
    output << "usage: aper [-t type] [-s seed] [-c scheme] [-n depth]\n"
              "\n"
              "Draw a finite substitution tiling as PDF.\n"
              "\n"
              "  -t, --tiling TYPE  p1, p2, p3, ammann-beenker, pinwheel,\n"
              "                     or stampfli (default: p3)\n"
              "                     aliases: pentagon-boat-star, kite-dart, rhomb,\n"
              "                     ab, stampfli-12-fold-1\n"
              "  -s, --seed NAME    choose the starting design\n"
              "                     p1: pentagon-5, pentagon-3, pentagon-2,\n"
              "                         diamond, boat, star (default: pentagon-5)\n"
              "                     p2: sun, star, ace, deuce, jack, queen, king\n"
              "                         (default: sun)\n"
              "                     p3: sun, star, thin, thick (default: sun;\n"
              "                         thin requires depth 2+)\n"
              "                     ammann-beenker: octagon, square, rhomb\n"
              "                         (default: octagon)\n"
              "                     pinwheel: triangle (default: triangle)\n"
              "                     stampfli: dodecagon, triangle, square, rhomb\n"
              "                         (default: dodecagon)\n"
              "  -c, --colour NAME  flare, grove, electric, or tide\n"
              "                     (default: flare)\n"
              "  -n, --depth N      subdivide N times (p1: 1-6, default 4;\n"
              "                     p2/p3: 1-12, default 7; ammann-beenker:\n"
              "                     1-6, default 4; pinwheel: 1-8, default 6;\n"
              "                     stampfli: 1-3, default 2)\n"
              "  -h, --help         show this help\n"
              "  -V, --version      show the version\n";
}

[[nodiscard]] aper::Seed parse_seed(std::string_view text) {
    if (text == "sun") {
        return aper::Seed::sun;
    }
    if (text == "star") {
        return aper::Seed::star;
    }
    if (text == "ace") {
        return aper::Seed::ace;
    }
    if (text == "deuce") {
        return aper::Seed::deuce;
    }
    if (text == "jack") {
        return aper::Seed::jack;
    }
    if (text == "queen") {
        return aper::Seed::queen;
    }
    if (text == "king") {
        return aper::Seed::king;
    }
    if (text == "thin") {
        return aper::Seed::thin;
    }
    if (text == "thick") {
        return aper::Seed::thick;
    }
    if (text == "pentagon-5") {
        return aper::Seed::pentagon_5;
    }
    if (text == "pentagon-3") {
        return aper::Seed::pentagon_3;
    }
    if (text == "pentagon-2") {
        return aper::Seed::pentagon_2;
    }
    if (text == "diamond") {
        return aper::Seed::diamond;
    }
    if (text == "boat") {
        return aper::Seed::boat;
    }
    if (text == "triangle") {
        return aper::Seed::triangle;
    }
    if (text == "square") {
        return aper::Seed::square;
    }
    if (text == "rhomb") {
        return aper::Seed::rhomb;
    }
    if (text == "octagon") {
        return aper::Seed::octagon;
    }
    if (text == "dodecagon") {
        return aper::Seed::dodecagon;
    }
    throw std::invalid_argument(
        "unknown seed: " + std::string(text));
}

[[nodiscard]] aper::ColourScheme parse_colour_scheme(std::string_view text) {
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

[[nodiscard]] aper::Tiling parse_tiling(std::string_view text) {
    if (text == "p1" || text == "pentagon-boat-star") {
        return aper::Tiling::p1;
    }
    if (text == "p2" || text == "kite-dart") {
        return aper::Tiling::p2;
    }
    if (text == "p3" || text == "rhomb") {
        return aper::Tiling::p3;
    }
    if (text == "ammann-beenker" || text == "ab") {
        return aper::Tiling::ammann_beenker;
    }
    if (text == "pinwheel") {
        return aper::Tiling::pinwheel;
    }
    if (text == "stampfli" || text == "stampfli-12-fold-1") {
        return aper::Tiling::stampfli;
    }
    throw std::invalid_argument(
        "tiling must be p1, p2, p3, ammann-beenker, pinwheel, or stampfli");
}

[[nodiscard]] aper::Seed default_seed(aper::Tiling tiling) {
    switch (tiling) {
    case aper::Tiling::p1:
        return aper::Seed::pentagon_5;
    case aper::Tiling::p2:
    case aper::Tiling::p3:
        return aper::Seed::sun;
    case aper::Tiling::ammann_beenker:
        return aper::Seed::octagon;
    case aper::Tiling::pinwheel:
        return aper::Seed::triangle;
    case aper::Tiling::stampfli:
        return aper::Seed::dodecagon;
    }
    throw std::invalid_argument("unknown tiling");
}

[[nodiscard]] unsigned default_depth(aper::Tiling tiling) {
    switch (tiling) {
    case aper::Tiling::p1:
        return aper::default_p1_depth;
    case aper::Tiling::p2:
    case aper::Tiling::p3:
        return aper::default_depth;
    case aper::Tiling::ammann_beenker:
        return aper::default_ammann_beenker_depth;
    case aper::Tiling::pinwheel:
        return aper::default_pinwheel_depth;
    case aper::Tiling::stampfli:
        return aper::default_stampfli_depth;
    }
    throw std::invalid_argument("unknown tiling");
}

[[nodiscard]] unsigned maximum_depth(aper::Tiling tiling) {
    switch (tiling) {
    case aper::Tiling::p1:
        return aper::maximum_p1_depth;
    case aper::Tiling::p2:
    case aper::Tiling::p3:
        return aper::maximum_depth;
    case aper::Tiling::ammann_beenker:
        return aper::maximum_ammann_beenker_depth;
    case aper::Tiling::pinwheel:
        return aper::maximum_pinwheel_depth;
    case aper::Tiling::stampfli:
        return aper::maximum_stampfli_depth;
    }
    throw std::invalid_argument("unknown tiling");
}

[[nodiscard]] unsigned parse_depth(std::string_view text) {
    unsigned depth = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), depth);
    if (error != std::errc{} || end != text.data() + text.size() ||
        depth < aper::minimum_depth) {
        throw std::invalid_argument("depth must be a positive integer");
    }
    return depth;
}

[[nodiscard]] Options parse_options(int argc, char** argv) {
    Options options;
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
        if (argument == "-s" || argument == "--seed") {
            if (++i == argc) {
                throw std::invalid_argument(argument == "-s"
                                                ? "option -s requires a seed"
                                                : "option --seed requires a seed");
            }
            options.seed = parse_seed(argv[i]);
            options.seed_selected = true;
            continue;
        }
        if (argument.starts_with("--seed=")) {
            options.seed = parse_seed(argument.substr(7));
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
            options.tiling = parse_tiling(argv[i]);
            continue;
        }
        if (argument.starts_with("--tiling=")) {
            options.tiling = parse_tiling(argument.substr(9));
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

    if (!options.seed_selected) {
        options.seed = default_seed(options.tiling);
    }
    if (!options.depth_selected) {
        options.depth = default_depth(options.tiling);
    }
    if (!aper::seed_supported(options.tiling, options.seed)) {
        throw std::invalid_argument(std::string(aper::seed_name(options.seed)) +
                                    " seed is not available for " +
                                    std::string(aper::tiling_name(options.tiling)));
    }
    const auto depth_limit = maximum_depth(options.tiling);
    if (options.depth > depth_limit) {
        throw std::invalid_argument(std::string(aper::tiling_name(options.tiling)) +
                                    " depth must be an integer from 1 to " +
                                    std::to_string(depth_limit));
    }
    if (options.tiling == aper::Tiling::p3 && options.seed == aper::Seed::thin &&
        options.depth == 1) {
        throw std::invalid_argument("thin seed requires a depth from 2 to 12");
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        const auto tiles =
            aper::generate_tiles(options.tiling, options.seed, options.depth);
        aper::write_pdf(std::cout, tiles, options.tiling, options.seed,
                        options.colour_scheme, options.depth);
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
