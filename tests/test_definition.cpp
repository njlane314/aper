#include "definition.hpp"

#include "discovery.hpp"
#include "library.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

int failures = 0;

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            std::cerr << __FILE__ << ':' << __LINE__                                   \
                      << ": check failed: " #condition "\n";                           \
            ++failures;                                                                \
        }                                                                              \
    } while (false)

constexpr std::string_view definition = R"aper(aper 1
id thue-file
name "Thue \"file\""
alias thue-example
inflation 2
depths 1 3 6
default-seed a
deduplicate false
source "Local" "thue-file" "https://example.test/thue" "A \\ citation" "https://example.test/licence"

tile a 0
polygon 0 0  1 0  1 1  0 1
end

tile b 15
polygon 0 0  1 0  1 1  0 1
end

rule a
child a 0 0 0 normal
child b 1 0 0 normal
child b 0 1 0 normal
child a 1 1 0 normal
end

rule b
child b 0 0 0 normal
child a 1 0 0 normal
child a 0 1 0 normal
child b 1 1 0 normal
end

seed a 1
place a 0 0 1 0 normal
end

seed turned 2
place b 1 -2 0.5 90 reflected
end
)aper";

aper::TilingSystem read(std::string_view text, std::string_view source = "fixture") {
    std::istringstream input{std::string(text)};
    return aper::SystemReader{}.read(input, source);
}

bool rejected(std::string text, std::string_view expected) {
    try {
        (void)read(text, "bad.aper");
    } catch (const std::invalid_argument& error) {
        return std::string_view(error.what()).find(expected) != std::string_view::npos;
    }
    return false;
}

void test_definition() {
    const auto system = read(definition);
    CHECK(system.spec().id == "thue-file");
    CHECK(system.spec().name == "Thue \"file\"");
    CHECK(system.spec().aliases.size() == 1);
    CHECK(system.spec().aliases.front() == "thue-example");
    CHECK(system.spec().sources.size() == 1);
    CHECK(system.spec().sources.front().citation == "A \\ citation");
    CHECK(system.prototiles().size() == 2);
    CHECK(system.prototile(0).shape == aper::Shape::generic_polygon);
    CHECK(system.rule().replacement(0).size() == 4);
    CHECK(system.rule().replacement(0).placements()[1].pose.translation() ==
          aper::Point(0.5, 0.0));
    CHECK(system.generate_raw("a", 3).size() == 64);
    const auto& turned = system.seed("turned").patch.placements().front().pose;
    CHECK(turned.reflected());
    CHECK(std::abs(turned.multiplier() - aper::Point(0.0, 0.5)) < 1.0e-12);

    std::ostringstream first;
    std::ostringstream second;
    aper::SystemWriter{}.write(first, system);
    aper::SystemWriter{}.write(second, system);
    CHECK(first.str() == second.str());
    CHECK(first.str().starts_with("aper 1\nid thue-file\n"));
    CHECK(first.str().find("child b 1 0 0 normal") != std::string::npos);
    CHECK(first.str().find("place b 1 -2 0.5 90 reflected") != std::string::npos);

    const auto round_trip = read(first.str(), "round-trip");
    CHECK(aper::canonical_key(round_trip) == aper::canonical_key(system));
    CHECK(round_trip.spec().sources.front().citation == "A \\ citation");

    auto unknown = std::string(definition);
    const auto child = unknown.find("child a 0 0 0 normal");
    unknown.replace(child, std::string_view("child a 0 0 0 normal").size(),
                    "child missing 0 0 0 normal");
    CHECK(rejected(std::move(unknown), "refers to unknown tile \"missing\""));

    auto gap = std::string(definition);
    const auto missing = gap.find("child a 1 1 0 normal\n");
    gap.erase(missing, std::string_view("child a 1 1 0 normal\n").size());
    CHECK(rejected(std::move(gap), "areas do not match"));
    CHECK(rejected("id no-header\n", "begin with aper 1"));
    CHECK(rejected("aper 1\nid unterminated\nname \"bad\n", "unterminated quoted"));
}

void test_shipped_definitions() {
    const auto check = [](std::string_view path) {
        std::ifstream input{std::string(path)};
        CHECK(input.good());
        if (!input) {
            return;
        }
        const auto system = aper::SystemReader{}.read(input, path);
        CHECK(system.validate().empty());
        CHECK((aper::GeometricValidator{{1.0e-9, 5, 2048}}.validate(system).valid()));
    };
    check("rules/chair.aper");
    check("rules/domino.aper");
}

void test_rule_library() {
    aper::RuleLibrary library;
    library.add_directory("rules");
    CHECK(library.catalogue().systems().size() == 9);
    CHECK(library.catalogue().get("squar-chair").spec().id == "square-chair");
    CHECK(library.catalogue().get("squiral-block").rule().inflation() == 3.0);
    CHECK(library.catalogue().get("period-tripling-2d").prototiles().size() == 2);

    library.add_fallback(aper::tiling_catalogue());
    CHECK(library.catalogue().systems().size() == 15);
    CHECK(library.catalogue().get("chair").spec().sources.front().record == "chair");
    CHECK(library.catalogue().get("p3").spec().id == "p3");
}

} // namespace

int main() {
    test_definition();
    test_shipped_definitions();
    test_rule_library();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "definition tests passed\n";
    return EXIT_SUCCESS;
}
