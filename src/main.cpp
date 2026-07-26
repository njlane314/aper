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

constexpr std::string_view version = "0.3.0";

struct Options {
    aper::Tiling tiling = aper::Tiling::p3;
    aper::ColourScheme colour_scheme = aper::default_colour_scheme;
    unsigned depth = aper::default_depth;
};

void help(std::ostream& output) {
    output << "usage: aper [-t p2|p3] [-c scheme] [-n depth]\n"
              "\n"
              "Draw a finite P2 or P3 Penrose tiling as PDF.\n"
              "\n"
              "  -t, --tiling TYPE  p2 kite-and-dart or p3 rhombs (default: p3)\n"
              "  -c, --colour NAME  flare, grove, electric, or tide\n"
              "                     (default: flare)\n"
              "  -n, --depth N      subdivide N times (1-12; default: 7)\n"
              "  -h, --help         show this help\n"
              "  -V, --version      show the version\n";
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
    if (text == "p2") {
        return aper::Tiling::p2;
    }
    if (text == "p3") {
        return aper::Tiling::p3;
    }
    throw std::invalid_argument("tiling must be p2 or p3");
}

[[nodiscard]] unsigned parse_depth(std::string_view text) {
    unsigned depth = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), depth);
    if (error != std::errc{} || end != text.data() + text.size() ||
        depth < aper::minimum_depth || depth > aper::maximum_depth) {
        throw std::invalid_argument("depth must be an integer from 1 to 12");
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
            continue;
        }
        if (argument.starts_with("--depth=")) {
            options.depth = parse_depth(argument.substr(8));
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
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        const auto triangles = aper::generate(options.tiling, options.depth);
        const auto tiles = aper::pair_tiles(triangles, options.tiling);
        aper::write_pdf(std::cout, tiles, options.tiling, options.colour_scheme,
                        options.depth);
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
