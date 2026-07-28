#include "bank.hpp"
#include "definition.hpp"
#include "discovery.hpp"
#include "library.hpp"
#include "pdf.hpp"
#include "version.hpp"
#include "view.hpp"

#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

enum class SearchSpace {
    polyomino,
    square,
    binary_square,
};

struct Options {
    aper::ColourScheme colour_scheme = aper::default_colour_scheme;
    SearchSpace search_space = SearchSpace::polyomino;
    unsigned depth = 4;
    unsigned cells = 3;
    std::size_t candidate = 0;
    std::filesystem::path bank_directory;
    bool rule = false;
    bool definition = false;
    bool classify = false;
    bool list_known = false;
    bool list_candidates = false;
    bool depth_selected = false;
    bool colour_selected = false;
    bool cells_selected = false;
    bool space_selected = false;
    bool candidate_selected = false;
    bool bank_selected = false;
};

void help(std::ostream& output) {
    output << "usage: aper-search [--space name] [--cells n] [--candidate n]\n"
              "                   [-c scheme] [-n depth] [-r]\n"
              "       aper-search [--space name] [--cells n] [--candidate n]\n"
              "                   --classify\n"
              "       aper-search [--space name] [--cells n] [--candidate n]\n"
              "                   --definition\n"
              "       aper-search [--space name] [--cells n] --list-candidates\n"
              "       aper-search -l | --list-known\n"
              "       aper-search -h | --help\n"
              "       aper-search -V | --version\n"
              "\n"
              "Search a bounded substitution space and inspect one survivor.\n"
              "\n"
              "The polyomino space enumerates every free connected polyomino of\n"
              "the selected size and uses exact cover to dissect its twofold\n"
              "inflation into four rotated or reflected copies. Square and\n"
              "binary-square remain explicit pipeline controls. Unmatched\n"
              "candidates do not establish novelty or aperiodicity.\n"
              "Exact classification uses independently encoded rules only; an\n"
              "unmatched survivor is not evidence of novelty.\n"
              "\n"
              "      --space NAME  polyomino, square, or binary-square\n"
              "                     (default: polyomino)\n"
              "      --cells N     polyomino cell count, 1-6 (default: 3)\n"
              "      --candidate N select zero-based survivor N (default: 0)\n"
              "      --bank DIR    use declarative .aper rules below DIR\n"
              "                     (default: ./rules, then the installed library)\n"
              "      --list-candidates\n"
              "                     list every survivor and exact bank match\n"
              "  -c, --colour NAME  flare, grove, electric, or tide\n"
              "                     (default: flare)\n"
              "  -n, --depth N      set patch substitution depth, 1-7\n"
              "                     (default: 4; patch output only)\n"
              "  -r, --rule         render the discovered substitution rule\n"
              "      --definition   write the selected candidate as .aper text\n"
              "  -k, --classify     check the selected survivor against the bank\n"
              "  -l, --list-known   list the encoded known-rule bank\n"
              "  -h, --help         show this help\n"
              "  -V, --version      show the version\n";
}

SearchSpace parse_search_space(std::string_view text) {
    if (text == "polyomino") {
        return SearchSpace::polyomino;
    }
    if (text == "square") {
        return SearchSpace::square;
    }
    if (text == "binary-square") {
        return SearchSpace::binary_square;
    }
    throw std::invalid_argument(
        "search space must be polyomino, square, or binary-square");
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

unsigned parse_cells(std::string_view text) {
    unsigned cells = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), cells);
    if (error != std::errc{} || end != text.data() + text.size() || cells == 0) {
        throw std::invalid_argument("cell count must be a positive integer");
    }
    return cells;
}

std::size_t parse_candidate(std::string_view text) {
    std::size_t candidate = 0;
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), candidate);
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::invalid_argument("candidate must be a non-negative integer");
    }
    return candidate;
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
        if (argument == "--definition") {
            options.definition = true;
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
        if (argument == "--list-candidates") {
            options.list_candidates = true;
            continue;
        }
        if (argument == "--space") {
            if (++i == argc) {
                throw std::invalid_argument("option --space requires a name");
            }
            options.search_space = parse_search_space(argv[i]);
            options.space_selected = true;
            continue;
        }
        if (argument.starts_with("--space=")) {
            options.search_space = parse_search_space(argument.substr(8));
            options.space_selected = true;
            continue;
        }
        if (argument == "--cells") {
            if (++i == argc) {
                throw std::invalid_argument("option --cells requires a count");
            }
            options.cells = parse_cells(argv[i]);
            options.cells_selected = true;
            continue;
        }
        if (argument.starts_with("--cells=")) {
            options.cells = parse_cells(argument.substr(8));
            options.cells_selected = true;
            continue;
        }
        if (argument == "--candidate") {
            if (++i == argc) {
                throw std::invalid_argument("option --candidate requires an index");
            }
            options.candidate = parse_candidate(argv[i]);
            options.candidate_selected = true;
            continue;
        }
        if (argument == "--bank") {
            if (++i == argc) {
                throw std::invalid_argument("option --bank requires a directory");
            }
            options.bank_directory = argv[i];
            options.bank_selected = true;
            continue;
        }
        if (argument.starts_with("--bank=")) {
            options.bank_directory = argument.substr(7);
            options.bank_selected = true;
            continue;
        }
        if (argument.starts_with("--candidate=")) {
            options.candidate = parse_candidate(argument.substr(12));
            options.candidate_selected = true;
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
    if (options.classify && (options.rule || options.definition || options.list_known ||
                             options.list_candidates || options.depth_selected ||
                             options.colour_selected)) {
        throw std::invalid_argument("classification does not render PDF output");
    }
    if (options.definition &&
        (options.rule || options.list_known || options.list_candidates ||
         options.depth_selected || options.colour_selected)) {
        throw std::invalid_argument(
            "definition output does not accept PDF or listing options");
    }
    if (options.list_known &&
        (options.rule || options.definition || options.list_candidates ||
         options.depth_selected || options.colour_selected || options.space_selected ||
         options.cells_selected || options.candidate_selected)) {
        throw std::invalid_argument("known-rule listing takes no output options");
    }
    if (options.list_candidates &&
        (options.rule || options.definition || options.depth_selected ||
         options.colour_selected || options.candidate_selected)) {
        throw std::invalid_argument(
            "candidate listing takes only search-space and cell-count options");
    }
    if (options.depth < 1 || options.depth > 7) {
        throw std::invalid_argument("depth must be an integer from 1 to 7");
    }
    if (options.search_space != SearchSpace::polyomino && options.cells_selected) {
        throw std::invalid_argument("--cells is only available for polyomino search");
    }
    if (options.search_space == SearchSpace::polyomino &&
        (options.cells < 1 || options.cells > 6)) {
        throw std::invalid_argument("polyomino cell count must be from 1 to 6");
    }
    return options;
}

aper::DiscoveryResult search(SearchSpace space, unsigned cells) {
    if (space == SearchSpace::polyomino) {
        const aper::DiscoveryOptions options{
            {1.0e-9, 5, 2048},
            4096,
            4096,
            true,
        };
        return aper::DiscoveryEngine{options}.run(aper::PolyominoRepTileSearch{cells});
    }
    if (space == SearchSpace::square) {
        return aper::DiscoveryEngine{}.run(aper::SquareLatticeSearch{});
    }
    const aper::DiscoveryOptions options{
        {1.0e-9, 3, 512},
        256,
        64,
        true,
    };
    return aper::DiscoveryEngine{options}.run(aper::BinarySquareSearch{});
}

std::string_view search_space_name(SearchSpace space) {
    switch (space) {
    case SearchSpace::polyomino:
        return "polyomino";
    case SearchSpace::square:
        return "square";
    case SearchSpace::binary_square:
        return "binary-square";
    }
    throw std::invalid_argument("unknown search space");
}

void list_known(std::ostream& output, const aper::TilingCatalogue& catalogue) {
    for (const auto& system : catalogue.systems()) {
        for (const auto& source : system.spec().sources) {
            output << system.spec().id << '\t' << source.record << '\t' << source.url
                   << '\n';
        }
    }
}

std::vector<aper::KnownTilingMatch> bank_matches(const aper::KnownTilingBank& bank,
                                                 const aper::TilingSystem& candidate,
                                                 SearchSpace space) {
    if (space == SearchSpace::binary_square) {
        return bank.classify(candidate, aper::BinarySquareSearch{});
    }
    return bank.classify(candidate);
}

void classify(std::ostream& output, const aper::TilingSystem& candidate,
              SearchSpace space, const aper::TilingCatalogue& catalogue) {
    const aper::KnownTilingBank bank{catalogue};
    const auto matches = bank_matches(bank, candidate, space);
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

void list_candidates(std::ostream& output, SearchSpace space, unsigned cells,
                     const aper::DiscoveryResult& result,
                     const aper::TilingCatalogue& catalogue) {
    const aper::KnownTilingBank bank{catalogue};
    const auto& statistics = result.statistics;
    output << "# search-space\t" << search_space_name(space) << '\n';
    if (space == SearchSpace::polyomino) {
        output << "# cells\t" << cells << '\n';
    }
    output << "# generated\t" << statistics.generated << '\n'
           << "# structurally-valid\t" << statistics.structurally_valid << '\n'
           << "# algebraically-valid\t" << statistics.algebraically_valid << '\n'
           << "# geometrically-valid\t" << statistics.geometrically_valid << '\n'
           << "# canonicalised\t" << statistics.canonicalised << '\n'
           << "# unique\t" << statistics.unique << '\n'
           << "# encoded-bank\t" << bank.systems().size() << '\n'
           << "index\tsystem\texact_bank_match\n";
    for (std::size_t index = 0; index < result.candidates.size(); ++index) {
        const auto& system = result.candidates[index].system;
        const auto matches = bank_matches(bank, system, space);
        output << index << '\t' << system.spec().id << '\t';
        if (matches.empty()) {
            output << '-';
        } else {
            for (std::size_t match = 0; match < matches.size(); ++match) {
                if (match != 0) {
                    output << ',';
                }
                output << matches[match].system->spec().id;
            }
        }
        output << '\n';
    }
}

aper::RuleLibrary load_bank(const Options& options) {
    aper::RuleLibrary library;
    if (options.bank_selected) {
        library.add_directory(options.bank_directory);
    } else {
        library.add_default_directory();
    }
    library.add_fallback(aper::tiling_catalogue());
    return library;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        const auto library = load_bank(options);
        const auto& catalogue = library.catalogue();
        if (options.list_known) {
            list_known(std::cout, catalogue);
            if (!std::cout) {
                throw std::runtime_error("could not write known-rule listing");
            }
            return 0;
        }
        const auto result = search(options.search_space, options.cells);
        if (options.search_space == SearchSpace::polyomino && options.cells == 3 &&
            result.candidates.size() != 2) {
            throw std::runtime_error(
                "the triomino search did not find exactly two candidates");
        }
        if (options.search_space == SearchSpace::square &&
            result.candidates.size() != 1) {
            throw std::runtime_error(
                "the square control search did not find exactly one candidate");
        }
        if (options.list_candidates) {
            list_candidates(std::cout, options.search_space, options.cells, result,
                            catalogue);
            if (!std::cout) {
                throw std::runtime_error("could not write candidate listing");
            }
            return 0;
        }
        if (options.candidate >= result.candidates.size()) {
            throw std::invalid_argument(
                "candidate index is outside the surviving search results");
        }
        const auto& candidate = result.candidates[options.candidate].system;
        if (options.classify) {
            classify(std::cout, candidate, options.search_space, catalogue);
            if (!std::cout) {
                throw std::runtime_error("could not write classification");
            }
            return 0;
        }
        if (options.definition) {
            aper::SystemWriter{}.write(std::cout, candidate);
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
