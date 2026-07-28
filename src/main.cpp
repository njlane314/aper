#include "definition.hpp"
#include "library.hpp"
#include "pdf.hpp"
#include "system.hpp"
#include "version.hpp"
#include "view.hpp"

#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

struct Options {
    std::string tiling = "p3";
    std::string file;
    std::filesystem::path library_directory;
    std::string seed;
    aper::ColourScheme colour_scheme = aper::default_colour_scheme;
    unsigned depth = 0;
    bool rule = false;
    bool definition = false;
    bool tiling_selected = false;
    bool file_selected = false;
    bool library_selected = false;
    bool seed_selected = false;
    bool depth_selected = false;
};

std::string system_names(const aper::TilingCatalogue& catalogue) {
    std::string names;
    const auto& systems = catalogue.systems();
    for (std::size_t i = 0; i < systems.size(); ++i) {
        if (i != 0) {
            names += i + 1 == systems.size() ? ", or " : ", ";
        }
        names += systems[i].spec().id;
    }
    return names;
}

void help(std::ostream& output) {
    aper::RuleLibrary library;
    library.add_default_directory();
    library.add_fallback(aper::tiling_catalogue());
    const auto& catalogue = library.catalogue();

    output << "usage: aper [-t type | -f file] [-s seed] [-c scheme] [-n depth] [-r]\n"
              "       aper [-t type | -f file] --definition\n"
              "\n"
              "Draw a substitution-tiling patch or rule sheet as PDF.\n"
              "\n"
              "  -t, --tiling TYPE  choose a library tiling system (default: p3)\n";
    for (const auto& system : catalogue.systems()) {
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

    output << "  -f, --file FILE    read a declarative .aper system; - means stdin\n"
              "  -L, --library DIR add every .aper definition below DIR\n"
              "                     (default: ./rules, then the installed library)\n"
              "  -s, --seed NAME    choose the patch's starting design\n";
    for (const auto& system : catalogue.systems()) {
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
    for (const auto& system : catalogue.systems()) {
        const auto depth = system.spec().depths;
        output << "                     " << system.spec().id << ": " << depth.minimum
               << '-' << depth.maximum << " (default: " << depth.recommended << ")\n";
    }
    output
        << "  -r, --rule         draw every prototile substitution rule\n"
           "                     (seed and depth are patch-only)\n"
           "      --definition   write the selected system as normalised .aper text\n"
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

const aper::TilingSystem& parse_tiling(std::string_view text,
                                       const aper::TilingCatalogue& catalogue) {
    const auto* system = catalogue.find(text);
    if (system == nullptr) {
        throw std::invalid_argument("tiling must be " + system_names(catalogue));
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

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];

        if (argument == "-h" || argument == "--help") {
            help(std::cout);
            std::exit(0);
        }
        if (argument == "-V" || argument == "--version") {
            std::cout << "aper " << aper::version << '\n';
            std::exit(0);
        }
        if (argument == "-r" || argument == "--rule") {
            options.rule = true;
            continue;
        }
        if (argument == "--definition") {
            options.definition = true;
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
            options.tiling = argv[i];
            options.tiling_selected = true;
            continue;
        }
        if (argument.starts_with("--tiling=")) {
            options.tiling = argument.substr(9);
            options.tiling_selected = true;
            continue;
        }
        if (argument == "-f" || argument == "--file") {
            if (++i == argc) {
                throw std::invalid_argument(argument == "-f"
                                                ? "option -f requires a file"
                                                : "option --file requires a file");
            }
            options.file = argv[i];
            options.file_selected = true;
            continue;
        }
        if (argument.starts_with("--file=")) {
            options.file = argument.substr(7);
            options.file_selected = true;
            continue;
        }
        if (argument == "-L" || argument == "--library") {
            if (++i == argc) {
                throw std::invalid_argument(
                    argument == "-L" ? "option -L requires a directory"
                                     : "option --library requires a directory");
            }
            options.library_directory = argv[i];
            options.library_selected = true;
            continue;
        }
        if (argument.starts_with("--library=")) {
            options.library_directory = argument.substr(10);
            options.library_selected = true;
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
    if (options.file_selected && options.file.empty()) {
        throw std::invalid_argument("file must not be empty");
    }
    if (options.file_selected && options.tiling_selected) {
        throw std::invalid_argument("file and tiling options are mutually exclusive");
    }
    if (options.file_selected && options.library_selected) {
        throw std::invalid_argument("file and library options are mutually exclusive");
    }
    if (options.rule && options.definition) {
        throw std::invalid_argument(
            "rule and definition outputs are mutually exclusive");
    }
    if (options.rule || options.definition) {
        if (options.seed_selected || options.depth_selected) {
            throw std::invalid_argument(
                "seed and depth options are only available for patch output");
        }
    }
    return options;
}

void resolve_patch_options(Options& options, const aper::TilingSystem& system) {
    if (options.rule || options.definition) {
        return;
    }

    if (!options.seed_selected) {
        options.seed = system.spec().default_seed;
    }
    if (!options.depth_selected) {
        options.depth = system.spec().depths.recommended;
    }
    const auto* seed = system.find_seed(options.seed);
    if (seed == nullptr) {
        throw std::invalid_argument(options.seed + " seed is not available for " +
                                    system.spec().name);
    }
    const auto depths = system.spec().depths;
    if (options.depth < depths.minimum || options.depth > depths.maximum) {
        throw std::invalid_argument(
            system.spec().name + " depth must be an integer from " +
            std::to_string(depths.minimum) + " to " + std::to_string(depths.maximum));
    }
    if (options.depth < seed->minimum_depth) {
        throw std::invalid_argument(seed->name + " seed requires a depth from " +
                                    std::to_string(seed->minimum_depth) + " to " +
                                    std::to_string(depths.maximum));
    }
}

aper::TilingSystem read_system(std::string_view file) {
    if (file == "-") {
        return aper::SystemReader{}.read(std::cin, "standard input");
    }
    std::ifstream input{std::string(file)};
    if (!input) {
        throw std::invalid_argument("could not open tiling definition: " +
                                    std::string(file));
    }
    return aper::SystemReader{}.read(input, file);
}

} // namespace

int main(int argc, char** argv) {
    try {
        auto options = parse_options(argc, argv);
        std::optional<aper::TilingSystem> loaded;
        aper::RuleLibrary library;
        if (options.file_selected) {
            loaded.emplace(read_system(options.file));
        } else {
            if (options.library_selected) {
                library.add_directory(options.library_directory);
            } else {
                library.add_default_directory();
            }
            library.add_fallback(aper::tiling_catalogue());
        }
        const auto& system = loaded.has_value()
                                 ? *loaded
                                 : parse_tiling(options.tiling, library.catalogue());
        resolve_patch_options(options, system);
        if (options.definition) {
            aper::SystemWriter{}.write(std::cout, system);
        } else {
            const auto drawing =
                options.rule
                    ? aper::RuleView{system}.drawing()
                    : aper::PatchView{system, options.seed, options.depth}.drawing();
            aper::PdfRenderer{}.write(std::cout, drawing, options.colour_scheme);
        }
        if (!std::cout) {
            throw std::runtime_error("could not write output to standard output");
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
