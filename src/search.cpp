#include "bank.hpp"
#include "discovery.hpp"
#include "pdf.hpp"
#include "version.hpp"
#include "view.hpp"

#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

enum class SearchSpace {
    square,
    binary_square,
};

struct Options {
    aper::ColourScheme colour_scheme = aper::default_colour_scheme;
    SearchSpace search_space = SearchSpace::square;
    unsigned depth = 4;
    std::size_t candidate = 0;
    bool rule = false;
    bool classify = false;
    bool list_known = false;
    bool list_candidates = false;
    bool depth_selected = false;
    bool colour_selected = false;
    bool space_selected = false;
    bool candidate_selected = false;
};

void help(std::ostream& output) {
    output << "usage: aper-search [--space name] [--candidate n] [-c scheme]\n"
              "                   [-n depth] [-r]\n"
              "       aper-search [--space name] [--candidate n] --classify\n"
              "       aper-search [--space name] --list-candidates\n"
              "       aper-search -l | --list-known\n"
              "       aper-search -h | --help\n"
              "       aper-search -V | --version\n"
              "\n"
              "Search a bounded substitution space and render one survivor as PDF.\n"
              "\n"
              "The square control covers a unit square with half-scale copies.\n"
              "The binary-square space exhausts all pairs of two-colour 2x2\n"
              "substitutions. Both test the discovery pipeline; unmatched\n"
              "candidates do not establish novelty or aperiodicity.\n"
              "Exact classification uses independently encoded rules only; an\n"
              "unmatched survivor is not evidence of novelty.\n"
              "\n"
              "      --space NAME  square or binary-square (default: square)\n"
              "      --candidate N render zero-based survivor N (default: 0)\n"
              "      --list-candidates\n"
              "                     list every survivor and exact bank match\n"
              "  -c, --colour NAME  flare, grove, electric, or tide\n"
              "                     (default: flare)\n"
              "  -n, --depth N      set patch substitution depth, 1-7\n"
              "                     (default: 4; patch output only)\n"
              "  -r, --rule         render the discovered substitution rule\n"
              "  -k, --classify     check the selected survivor against the bank\n"
              "  -l, --list-known   list the encoded known-rule bank\n"
              "  -h, --help         show this help\n"
              "  -V, --version      show the version\n";
}

SearchSpace parse_search_space(std::string_view text) {
    if (text == "square") {
        return SearchSpace::square;
    }
    if (text == "binary-square") {
        return SearchSpace::binary_square;
    }
    throw std::invalid_argument("search space must be square or binary-square");
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
        if (argument == "--candidate") {
            if (++i == argc) {
                throw std::invalid_argument("option --candidate requires an index");
            }
            options.candidate = parse_candidate(argv[i]);
            options.candidate_selected = true;
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
    if (options.classify &&
        (options.rule || options.list_known || options.list_candidates ||
         options.depth_selected || options.colour_selected)) {
        throw std::invalid_argument("classification does not render PDF output");
    }
    if (options.list_known && (options.rule || options.list_candidates ||
                               options.depth_selected || options.colour_selected ||
                               options.space_selected || options.candidate_selected)) {
        throw std::invalid_argument("known-rule listing takes no output options");
    }
    if (options.list_candidates &&
        (options.rule || options.depth_selected || options.colour_selected ||
         options.candidate_selected)) {
        throw std::invalid_argument(
            "candidate listing takes only the search-space option");
    }
    if (options.depth < 1 || options.depth > 7) {
        throw std::invalid_argument("depth must be an integer from 1 to 7");
    }
    return options;
}

aper::DiscoveryResult search(SearchSpace space) {
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
    return space == SearchSpace::square ? "square" : "binary-square";
}

void list_known(std::ostream& output) {
    for (const auto& system : aper::tiling_catalogue().systems()) {
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
              SearchSpace space) {
    const aper::KnownTilingBank bank{aper::tiling_catalogue()};
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

void list_candidates(std::ostream& output, SearchSpace space,
                     const aper::DiscoveryResult& result) {
    const aper::KnownTilingBank bank{aper::tiling_catalogue()};
    const auto& statistics = result.statistics;
    output << "# search-space\t" << search_space_name(space) << '\n'
           << "# generated\t" << statistics.generated << '\n'
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
        const auto result = search(options.search_space);
        if (options.search_space == SearchSpace::square &&
            result.candidates.size() != 1) {
            throw std::runtime_error(
                "the square control search did not find exactly one candidate");
        }
        if (options.list_candidates) {
            list_candidates(std::cout, options.search_space, result);
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
            classify(std::cout, candidate, options.search_space);
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
