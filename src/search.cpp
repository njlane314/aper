#include "bank.hpp"
#include "discovery.hpp"
#include "pdf.hpp"
#include "version.hpp"
#include "view.hpp"

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace {

struct Options {
    aper::ColourScheme colour_scheme = aper::default_colour_scheme;
    unsigned depth = 4;
    bool rule = false;
    bool classify = false;
    bool list_known = false;
    bool depth_selected = false;
    bool colour_selected = false;
};

void help(std::ostream& output) {
    output << "usage: aper-search [-c scheme] [-n depth] [-r]\n"
              "       aper-search -k | --classify\n"
              "       aper-search -l | --list-known\n"
              "       aper-search -h | --help\n"
              "       aper-search -V | --version\n"
              "\n"
              "Search a bounded substitution space and render its survivor as PDF.\n"
              "\n"
              "The current control search exactly covers one unit square with\n"
              "half-scale squares on a 2x2 grid. It tests the discovery pipeline;\n"
              "it does not claim novelty or aperiodicity.\n"
              "Exact classification uses independently encoded rules only; an\n"
              "unmatched survivor is not evidence of novelty.\n"
              "\n"
              "  -c, --colour NAME  flare, grove, electric, or tide\n"
              "                     (default: flare)\n"
              "  -n, --depth N      set patch substitution depth, 1-7\n"
              "                     (default: 4; patch output only)\n"
              "  -r, --rule         render the discovered substitution rule\n"
              "  -k, --classify     check the survivor against encoded known rules\n"
              "  -l, --list-known   list the encoded known-rule bank\n"
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
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "-h" || argument == "--help") {
            help(std::cout);
            std::exit(0);
        }
        if (argument == "-V" || argument == "--version") {
            std::cout << "aper-search " << aper::version << '\n';
            std::exit(0);
        }
        if (argument == "-r" || argument == "--rule") {
            options.rule = true;
            continue;
        }
        if (argument == "-k" || argument == "--classify") {
            options.classify = true;
            continue;
        }
        if (argument == "-l" || argument == "--list-known") {
            options.list_known = true;
            continue;
        }
        if (argument == "-c" || argument == "--colour") {
            if (++i == argc) {
                throw std::invalid_argument(
                    argument == "-c" ? "option -c requires a colour scheme"
                                     : "option --colour requires a colour scheme");
            }
            options.colour_scheme = parse_colour_scheme(argv[i]);
            options.colour_selected = true;
            continue;
        }
        if (argument.starts_with("--colour=")) {
            options.colour_scheme = parse_colour_scheme(argument.substr(9));
            options.colour_selected = true;
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
                throw std::invalid_argument("aper-search takes no operands");
            }
            break;
        }
        if (argument.starts_with('-')) {
            throw std::invalid_argument("unknown option: " + std::string(argument));
        }
        throw std::invalid_argument("aper-search takes no operands");
    }

    if (options.rule && options.depth_selected) {
        throw std::invalid_argument("depth is only available for patch output");
    }
    if (options.classify && (options.rule || options.list_known ||
                             options.depth_selected || options.colour_selected)) {
        throw std::invalid_argument("classification does not render PDF output");
    }
    if (options.list_known &&
        (options.rule || options.depth_selected || options.colour_selected)) {
        throw std::invalid_argument("known-rule listing takes no output options");
    }
    if (options.depth < 1 || options.depth > 7) {
        throw std::invalid_argument("depth must be an integer from 1 to 7");
    }
    return options;
}

void list_known(std::ostream& output) {
    for (const auto& system : aper::tiling_catalogue().systems()) {
        for (const auto& source : system.spec().sources) {
            output << system.spec().id << '\t' << source.record << '\t' << source.url
                   << '\n';
        }
    }
}

void classify(std::ostream& output, const aper::TilingSystem& candidate) {
    const aper::KnownTilingBank bank{aper::tiling_catalogue()};
    const auto matches = bank.classify(candidate);
    if (matches.empty()) {
        output << "no-exact-match\t" << bank.systems().size()
               << " encoded systems checked\n";
        return;
    }
    for (const auto& match : matches) {
        for (const auto& source : match.system->spec().sources) {
            output << "exact-rule\t" << match.system->spec().id << '\t' << source.url
                   << '\n';
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        if (options.list_known) {
            list_known(std::cout);
            if (!std::cout) {
                throw std::runtime_error("could not write known-rule listing");
            }
            return 0;
        }
        const auto result = aper::DiscoveryEngine{}.run(aper::SquareLatticeSearch{});
        if (result.candidates.size() != 1) {
            throw std::runtime_error(
                "the square control search did not find exactly one candidate");
        }
        const auto& candidate = result.candidates.front().system;
        if (options.classify) {
            classify(std::cout, candidate);
            if (!std::cout) {
                throw std::runtime_error("could not write classification");
            }
            return 0;
        }
        const auto drawing =
            options.rule ? aper::RuleView{candidate}.drawing()
                         : aper::PatchView{candidate, candidate.spec().default_seed,
                                           options.depth}
                               .drawing();
        aper::PdfRenderer{}.write(std::cout, drawing, options.colour_scheme);
        if (!std::cout) {
            throw std::runtime_error("could not write PDF to standard output");
        }
    } catch (const std::invalid_argument& error) {
        std::cerr << "aper-search: " << error.what()
                  << "\n"
                     "Try 'aper-search -h' for more information.\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "aper-search: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
