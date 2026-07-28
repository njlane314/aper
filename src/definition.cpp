#include "definition.hpp"

#include "discovery.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <istream>
#include <limits>
#include <locale>
#include <map>
#include <numbers>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aper {
namespace {

struct RawPlacement {
    std::string type;
    Point translation{};
    double scale = 1.0;
    double degrees = 0.0;
    bool reflected = false;
    std::size_t line = 0;
};

struct RawTile {
    std::string name;
    std::uint8_t fill = 0;
    std::vector<Point> boundary;
    std::size_t line = 0;
    std::size_t polygon_line = 0;
};

struct RawRule {
    std::string parent;
    std::vector<RawPlacement> children;
    std::size_t line = 0;
};

struct RawSeed {
    std::string name;
    unsigned minimum_depth = 1;
    std::vector<RawPlacement> placements;
    std::size_t line = 0;
};

struct RawDefinition {
    std::optional<std::string> id;
    std::optional<std::string> name;
    std::vector<std::string> aliases;
    std::optional<double> inflation;
    std::optional<DepthRange> depths;
    std::optional<std::string> default_seed;
    std::vector<SourceReference> sources;
    std::vector<RawTile> tiles;
    std::vector<RawRule> rules;
    std::vector<RawSeed> seeds;
    bool deduplicate = false;
    bool deduplicate_seen = false;
    bool version_seen = false;
};

bool identifier(std::string_view text) {
    if (text.empty()) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](char character) {
        return (character >= 'a' && character <= 'z') ||
               (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') || character == '-' ||
               character == '_' || character == '.';
    });
}

std::string quote(std::string_view text) {
    std::string result{"\""};
    for (const auto character : text) {
        if (character == '\\' || character == '"') {
            result += '\\';
        }
        result += character;
    }
    result += '"';
    return result;
}

std::string number(double value) {
    if (value == 0.0) {
        value = 0.0;
    }
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return output.str();
}

double degrees(Point multiplier) {
    return std::arg(multiplier) * 180.0 / std::numbers::pi;
}

Point polar(double scale, double degrees_value) {
    auto result = std::polar(scale, degrees_value * std::numbers::pi / 180.0);
    const auto threshold = 16.0 * std::numeric_limits<double>::epsilon() * scale;
    if (std::abs(result.real()) < threshold) {
        result.real(0.0);
    }
    if (std::abs(result.imag()) < threshold) {
        result.imag(0.0);
    }
    return result;
}

class Parser {
  public:
    explicit Parser(std::string_view source_name)
        : source_(source_name.empty() ? "<input>" : source_name) {}

    TilingSystem read(std::istream& input) {
        std::string line;
        while (std::getline(input, line)) {
            ++line_;
            consume(tokenise(line));
        }
        if (!input.eof()) {
            fail(0, "could not read the definition");
        }
        if (section_ != Section::none) {
            fail(section_line_, "unterminated " + section_name() + " section");
        }
        return build();
    }

  private:
    enum class Section {
        none,
        tile,
        rule,
        seed,
    };

    void fail(std::size_t line, const std::string& detail) const {
        auto message = source_;
        if (line != 0) {
            message += ':' + std::to_string(line);
        }
        throw std::invalid_argument(message + ": " + detail);
    }

    std::vector<std::string> tokenise(std::string_view line) const {
        std::vector<std::string> tokens;
        std::string token;
        bool active = false;
        bool in_quotes = false;
        bool quote_closed = false;
        for (std::size_t i = 0; i < line.size(); ++i) {
            const auto character = line[i];
            if (in_quotes) {
                if (character == '\\') {
                    if (++i == line.size() || (line[i] != '\\' && line[i] != '"')) {
                        fail(line_, "quoted strings allow only \\\\ and \\\"");
                    }
                    token += line[i];
                } else if (character == '"') {
                    in_quotes = false;
                    quote_closed = true;
                } else {
                    token += character;
                }
                continue;
            }
            if (character == '#') {
                break;
            }
            if (std::isspace(static_cast<unsigned char>(character)) != 0) {
                if (active) {
                    tokens.push_back(std::move(token));
                    token.clear();
                    active = false;
                    quote_closed = false;
                }
                continue;
            }
            if (quote_closed) {
                fail(line_, "a quoted string must be followed by whitespace");
            }
            if (character == '"') {
                if (active) {
                    fail(line_, "a quote may only begin a token");
                }
                active = true;
                in_quotes = true;
                continue;
            }
            active = true;
            token += character;
        }
        if (in_quotes) {
            fail(line_, "unterminated quoted string");
        }
        if (active) {
            tokens.push_back(std::move(token));
        }
        return tokens;
    }

    double real(std::string_view text, std::string_view role) const {
        std::istringstream input{std::string(text)};
        input.imbue(std::locale::classic());
        double value = 0.0;
        input >> value;
        if (!input || input.peek() != std::char_traits<char>::eof() ||
            !std::isfinite(value)) {
            fail(line_, std::string(role) + " must be a finite decimal number");
        }
        return value;
    }

    unsigned natural(std::string_view text, std::string_view role) const {
        unsigned value = 0;
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size() || value == 0) {
            fail(line_, std::string(role) + " must be a positive integer");
        }
        return value;
    }

    bool handedness(std::string_view text) const {
        if (text == "normal") {
            return false;
        }
        if (text == "reflected") {
            return true;
        }
        fail(line_, "handedness must be normal or reflected");
        return false;
    }

    void expect(const std::vector<std::string>& tokens, std::size_t size,
                std::string_view syntax) const {
        if (tokens.size() != size) {
            fail(line_, "expected " + std::string(syntax));
        }
    }

    void check_identifier(std::string_view text, std::string_view role) const {
        if (!identifier(text)) {
            fail(line_, std::string(role) + " is not a valid identifier");
        }
    }

    template <typename Value>
    void set_once(std::optional<Value>& target, Value value, std::string_view role) {
        if (target.has_value()) {
            fail(line_, "duplicate " + std::string(role));
        }
        target = std::move(value);
    }

    void consume(const std::vector<std::string>& tokens) {
        if (tokens.empty()) {
            return;
        }
        if (!definition_.version_seen) {
            expect(tokens, 2, "aper 1");
            if (tokens[0] != "aper" || tokens[1] != "1") {
                fail(line_, "a definition must begin with aper 1");
            }
            definition_.version_seen = true;
            return;
        }
        if (section_ != Section::none) {
            consume_section(tokens);
            return;
        }
        consume_header(tokens);
    }

    void consume_header(const std::vector<std::string>& tokens) {
        const auto& command = tokens[0];
        if (command == "id") {
            expect(tokens, 2, "id IDENTIFIER");
            check_identifier(tokens[1], "system id");
            set_once(definition_.id, tokens[1], "system id");
        } else if (command == "name") {
            expect(tokens, 2, "name \"DISPLAY NAME\"");
            if (tokens[1].empty()) {
                fail(line_, "display name must not be empty");
            }
            set_once(definition_.name, tokens[1], "display name");
        } else if (command == "alias") {
            expect(tokens, 2, "alias IDENTIFIER");
            check_identifier(tokens[1], "alias");
            definition_.aliases.push_back(tokens[1]);
        } else if (command == "inflation") {
            expect(tokens, 2, "inflation NUMBER");
            set_once(definition_.inflation, real(tokens[1], "inflation"), "inflation");
        } else if (command == "depths") {
            expect(tokens, 4, "depths MINIMUM RECOMMENDED MAXIMUM");
            set_once(definition_.depths,
                     DepthRange{natural(tokens[1], "minimum depth"),
                                natural(tokens[2], "recommended depth"),
                                natural(tokens[3], "maximum depth")},
                     "depth range");
        } else if (command == "default-seed") {
            expect(tokens, 2, "default-seed IDENTIFIER");
            check_identifier(tokens[1], "default seed");
            set_once(definition_.default_seed, tokens[1], "default seed");
        } else if (command == "deduplicate") {
            expect(tokens, 2, "deduplicate true|false");
            if (definition_.deduplicate_seen) {
                fail(line_, "duplicate deduplicate setting");
            }
            if (tokens[1] != "true" && tokens[1] != "false") {
                fail(line_, "deduplicate must be true or false");
            }
            definition_.deduplicate = tokens[1] == "true";
            definition_.deduplicate_seen = true;
        } else if (command == "source") {
            expect(tokens, 6, "source COLLECTION RECORD URL CITATION LICENCE-URL");
            definition_.sources.push_back(
                {tokens[1], tokens[2], tokens[3], tokens[4], tokens[5]});
        } else if (command == "tile") {
            begin_tile(tokens);
        } else if (command == "rule") {
            begin_rule(tokens);
        } else if (command == "seed") {
            begin_seed(tokens);
        } else {
            fail(line_, "unknown directive " + quote(command));
        }
    }

    void begin_tile(const std::vector<std::string>& tokens) {
        expect(tokens, 3, "tile IDENTIFIER FILL");
        check_identifier(tokens[1], "tile name");
        unsigned fill = 0;
        const auto [end, error] = std::from_chars(
            tokens[2].data(), tokens[2].data() + tokens[2].size(), fill);
        if (error != std::errc{} || end != tokens[2].data() + tokens[2].size() ||
            fill > maximum_fill) {
            fail(line_, "tile fill must be an integer from 0 to " +
                            std::to_string(maximum_fill));
        }
        definition_.tiles.push_back(
            {tokens[1], static_cast<std::uint8_t>(fill), {}, line_, 0});
        section_ = Section::tile;
        section_index_ = definition_.tiles.size() - 1;
        section_line_ = line_;
    }

    void begin_rule(const std::vector<std::string>& tokens) {
        expect(tokens, 2, "rule PARENT");
        check_identifier(tokens[1], "rule parent");
        definition_.rules.push_back({tokens[1], {}, line_});
        section_ = Section::rule;
        section_index_ = definition_.rules.size() - 1;
        section_line_ = line_;
    }

    void begin_seed(const std::vector<std::string>& tokens) {
        expect(tokens, 3, "seed IDENTIFIER MINIMUM-DEPTH");
        check_identifier(tokens[1], "seed name");
        definition_.seeds.push_back(
            {tokens[1], natural(tokens[2], "seed minimum depth"), {}, line_});
        section_ = Section::seed;
        section_index_ = definition_.seeds.size() - 1;
        section_line_ = line_;
    }

    void consume_section(const std::vector<std::string>& tokens) {
        if (tokens[0] == "end") {
            expect(tokens, 1, "end");
            finish_section();
            section_ = Section::none;
            return;
        }
        if (section_ == Section::tile) {
            consume_tile(tokens);
        } else if (section_ == Section::rule) {
            consume_rule(tokens);
        } else {
            consume_seed(tokens);
        }
    }

    void consume_tile(const std::vector<std::string>& tokens) {
        auto& tile = definition_.tiles[section_index_];
        if (tokens[0] != "polygon") {
            fail(line_, "a tile section accepts only polygon and end");
        }
        if (tile.polygon_line != 0) {
            fail(line_, "a tile may have only one polygon");
        }
        if (tokens.size() < 7 || (tokens.size() - 1) % 2 != 0) {
            fail(line_, "polygon needs at least three x y vertex pairs");
        }
        for (std::size_t i = 1; i < tokens.size(); i += 2) {
            tile.boundary.push_back({real(tokens[i], "polygon coordinate"),
                                     real(tokens[i + 1], "polygon coordinate")});
        }
        tile.polygon_line = line_;
    }

    void consume_rule(const std::vector<std::string>& tokens) {
        expect(tokens, 6, "child TYPE X Y DEGREES normal|reflected");
        if (tokens[0] != "child") {
            fail(line_, "a rule section accepts only child and end");
        }
        check_identifier(tokens[1], "child type");
        definition_.rules[section_index_].children.push_back(
            {tokens[1],
             {real(tokens[2], "child x coordinate"),
              real(tokens[3], "child y coordinate")},
             1.0,
             real(tokens[4], "child angle"),
             handedness(tokens[5]),
             line_});
    }

    void consume_seed(const std::vector<std::string>& tokens) {
        expect(tokens, 7, "place TYPE X Y SCALE DEGREES normal|reflected");
        if (tokens[0] != "place") {
            fail(line_, "a seed section accepts only place and end");
        }
        check_identifier(tokens[1], "placed type");
        const auto scale = real(tokens[4], "seed scale");
        if (scale <= 0.0) {
            fail(line_, "seed scale must be positive");
        }
        definition_.seeds[section_index_].placements.push_back(
            {tokens[1],
             {real(tokens[2], "seed x coordinate"),
              real(tokens[3], "seed y coordinate")},
             scale,
             real(tokens[5], "seed angle"),
             handedness(tokens[6]),
             line_});
    }

    void finish_section() {
        if (section_ == Section::tile &&
            definition_.tiles[section_index_].polygon_line == 0) {
            fail(section_line_, "tile has no polygon");
        }
        if (section_ == Section::rule &&
            definition_.rules[section_index_].children.empty()) {
            fail(section_line_, "rule has no children");
        }
        if (section_ == Section::seed &&
            definition_.seeds[section_index_].placements.empty()) {
            fail(section_line_, "seed has no placements");
        }
    }

    std::string section_name() const {
        if (section_ == Section::tile) {
            return "tile";
        }
        if (section_ == Section::rule) {
            return "rule";
        }
        return "seed";
    }

    template <typename Item, typename Name>
    void reject_duplicate_names(const std::vector<Item>& items, Name name,
                                std::string_view role) const {
        std::map<std::string, std::size_t> first_lines;
        for (const auto& item : items) {
            const auto [found, inserted] = first_lines.emplace(name(item), item.line);
            if (!inserted) {
                fail(item.line, "duplicate " + std::string(role) + ' ' +
                                    quote(name(item)) + "; first declared on line " +
                                    std::to_string(found->second));
            }
        }
    }

    TilingSystem build() {
        if (!definition_.version_seen) {
            fail(0, "empty definition; expected aper 1");
        }
        if (!definition_.id.has_value()) {
            fail(0, "missing system id");
        }
        if (!definition_.name.has_value()) {
            fail(0, "missing display name");
        }
        if (!definition_.inflation.has_value()) {
            fail(0, "missing inflation");
        }
        if (!definition_.depths.has_value()) {
            fail(0, "missing depth range");
        }
        if (!definition_.default_seed.has_value()) {
            fail(0, "missing default seed");
        }
        if (*definition_.inflation <= 1.0) {
            fail(0, "inflation must be greater than one");
        }
        if (definition_.tiles.empty()) {
            fail(0, "definition has no tiles");
        }
        reject_duplicate_names(
            definition_.tiles, [](const auto& tile) { return tile.name; }, "tile");
        reject_duplicate_names(
            definition_.rules, [](const auto& rule) { return rule.parent; },
            "rule parent");
        reject_duplicate_names(
            definition_.seeds, [](const auto& seed) { return seed.name; }, "seed");

        std::map<std::string, PrototileId> ids;
        std::vector<Prototile> prototiles;
        prototiles.reserve(definition_.tiles.size());
        for (const auto& tile : definition_.tiles) {
            const auto id = prototiles.size();
            ids.emplace(tile.name, id);
            prototiles.push_back(
                {id, tile.name, Shape::generic_polygon, tile.boundary, tile.fill});
        }
        const auto find_id = [&](std::string_view name, std::size_t line,
                                 std::string_view role) {
            const auto found = ids.find(std::string(name));
            if (found == ids.end()) {
                fail(line,
                     std::string(role) + " refers to unknown tile " + quote(name));
            }
            return found->second;
        };

        const auto inflation = *definition_.inflation;
        const auto scale = 1.0 / inflation;
        std::vector<RuleEntry> rules;
        rules.reserve(definition_.rules.size());
        for (const auto& rule : definition_.rules) {
            std::vector<Placement> children;
            children.reserve(rule.children.size());
            for (const auto& child : rule.children) {
                children.push_back(
                    {find_id(child.type, child.line, "child"),
                     Similarity{child.translation * scale, polar(scale, child.degrees),
                                child.reflected}});
            }
            rules.push_back({find_id(rule.parent, rule.line, "rule parent"),
                             Patch{std::move(children)}});
        }

        std::vector<SeedPatch> seeds;
        seeds.reserve(definition_.seeds.size());
        for (const auto& seed : definition_.seeds) {
            std::vector<Placement> placements;
            placements.reserve(seed.placements.size());
            for (const auto& placement : seed.placements) {
                placements.push_back(
                    {find_id(placement.type, placement.line, "seed placement"),
                     Similarity{placement.translation,
                                polar(placement.scale, placement.degrees),
                                placement.reflected}});
            }
            seeds.push_back(
                {seed.name, Patch{std::move(placements)}, seed.minimum_depth});
        }

        if (std::find(definition_.aliases.begin(), definition_.aliases.end(),
                      *definition_.id) != definition_.aliases.end()) {
            fail(0, "an alias duplicates the system id");
        }
        auto aliases = definition_.aliases;
        std::sort(aliases.begin(), aliases.end());
        if (std::adjacent_find(aliases.begin(), aliases.end()) != aliases.end()) {
            fail(0, "duplicate alias");
        }

        TilingSystem system{
            {*definition_.id, *definition_.name, std::move(definition_.aliases),
             *definition_.default_seed, *definition_.depths,
             std::move(definition_.sources)},
            std::move(prototiles),
            SubstitutionRule{inflation, std::move(rules), definition_.deduplicate},
            std::move(seeds)};
        if (!system.validate().empty()) {
            fail(0, system.validate().front());
        }
        if (!area_eigenvalue_matches(system)) {
            fail(0, "tile areas do not match the substitution incidence matrix");
        }
        constexpr std::size_t maximum_validation_tiles = 100000;
        const auto report =
            GeometricValidator{{1.0e-9, 1, maximum_validation_tiles}}.validate(system);
        if (!report.valid()) {
            fail(0, report.issues().front().detail);
        }
        return system;
    }

    std::string source_;
    std::size_t line_ = 0;
    RawDefinition definition_;
    Section section_ = Section::none;
    std::size_t section_index_ = 0;
    std::size_t section_line_ = 0;
};

void check_serialisable(const TilingSystem& system) {
    if (!system.validate().empty()) {
        throw std::invalid_argument("cannot write an invalid tiling system: " +
                                    system.validate().front());
    }
    const auto& spec = system.spec();
    if (!identifier(spec.id) || !identifier(spec.default_seed)) {
        throw std::invalid_argument(
            "cannot write a system with an invalid id or default seed");
    }
    for (const auto& alias : spec.aliases) {
        if (!identifier(alias)) {
            throw std::invalid_argument("cannot write invalid alias " + quote(alias));
        }
    }
    for (const auto& seed : system.seeds()) {
        if (!identifier(seed.name)) {
            throw std::invalid_argument("cannot write invalid seed name " +
                                        quote(seed.name));
        }
    }
    std::map<std::string, PrototileId> names;
    for (const auto& prototile : system.prototiles()) {
        if (!identifier(prototile.name)) {
            throw std::invalid_argument("cannot write invalid tile identifier " +
                                        quote(prototile.name));
        }
        if (!names.emplace(prototile.name, prototile.id).second) {
            throw std::invalid_argument("cannot write duplicate tile name " +
                                        quote(prototile.name));
        }
    }
}

} // namespace

TilingSystem SystemReader::read(std::istream& input,
                                std::string_view source_name) const {
    return Parser{source_name}.read(input);
}

void SystemWriter::write(std::ostream& output, const TilingSystem& system) const {
    check_serialisable(system);
    const auto& spec = system.spec();
    std::ostringstream encoded;
    encoded.imbue(std::locale::classic());
    encoded << "aper 1\n"
            << "id " << spec.id << '\n'
            << "name " << quote(spec.name) << '\n';
    for (const auto& alias : spec.aliases) {
        encoded << "alias " << alias << '\n';
    }
    encoded << "inflation " << number(system.rule().inflation()) << '\n'
            << "depths " << spec.depths.minimum << ' ' << spec.depths.recommended << ' '
            << spec.depths.maximum << '\n'
            << "default-seed " << spec.default_seed << '\n'
            << "deduplicate " << (system.rule().deduplicates() ? "true" : "false")
            << "\n\n";
    for (const auto& source : spec.sources) {
        encoded << "source " << quote(source.collection) << ' ' << quote(source.record)
                << ' ' << quote(source.url) << ' ' << quote(source.citation) << ' '
                << quote(source.licence_url) << '\n';
    }
    if (!spec.sources.empty()) {
        encoded << '\n';
    }
    for (const auto& prototile : system.prototiles()) {
        encoded << "tile " << prototile.name << ' '
                << static_cast<unsigned>(prototile.fill) << "\npolygon";
        for (const auto point : prototile.boundary) {
            encoded << ' ' << number(point.real()) << ' ' << number(point.imag());
        }
        encoded << "\nend\n\n";
    }

    const auto inflation = system.rule().inflation();
    for (const auto& rule : system.rule().entries()) {
        encoded << "rule " << system.prototile(rule.parent).name << '\n';
        for (const auto& child : rule.replacement.placements()) {
            const auto& pose = child.pose;
            const auto translation = inflation * pose.translation();
            encoded << "child " << system.prototile(child.prototile).name << ' '
                    << number(translation.real()) << ' ' << number(translation.imag())
                    << ' ' << number(degrees(pose.multiplier())) << ' '
                    << (pose.reflected() ? "reflected" : "normal") << '\n';
        }
        encoded << "end\n\n";
    }
    for (const auto& seed : system.seeds()) {
        encoded << "seed " << seed.name << ' ' << seed.minimum_depth << '\n';
        for (const auto& placement : seed.patch.placements()) {
            const auto& pose = placement.pose;
            encoded << "place " << system.prototile(placement.prototile).name << ' '
                    << number(pose.translation().real()) << ' '
                    << number(pose.translation().imag()) << ' '
                    << number(std::abs(pose.multiplier())) << ' '
                    << number(degrees(pose.multiplier())) << ' '
                    << (pose.reflected() ? "reflected" : "normal") << '\n';
        }
        encoded << "end\n";
        if (&seed != &system.seeds().back()) {
            encoded << '\n';
        }
    }

    const auto text = encoded.str();
    std::istringstream check{text};
    const auto round_trip = SystemReader{}.read(check, "generated definition");
    if (canonical_key(round_trip) != canonical_key(system)) {
        throw std::invalid_argument(
            "the tiling system cannot be represented exactly by aper 1");
    }
    output << text;
    if (!output) {
        throw std::runtime_error("could not write tiling definition");
    }
}

} // namespace aper
