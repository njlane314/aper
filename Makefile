CXX ?= c++
CPPFLAGS ?=
CXXFLAGS ?= -O2
LDFLAGS ?=
LDLIBS ?=

PREFIX ?= /usr/local
DESTDIR ?=
RULE_INSTALL_DIR = $(PREFIX)/share/aper/rules
RULE_FILES = $(sort $(wildcard rules/*.aper))

CPPFLAGS += -DAPER_RULES_DIRECTORY=\"$(RULE_INSTALL_DIR)\"

WARNINGS = -Wall -Wextra -Wpedantic -Wconversion -Wshadow
STANDARD = -std=c++20
BUILD = .build
PROGRAM = aper
SEARCH_PROGRAM = aper-search
PROGRAMS = $(PROGRAM) $(SEARCH_PROGRAM)
ENCYCLOPEDIA_BANK = data/encyclopedia.tsv
ENCYCLOPEDIA_NOTICE = data/encyclopedia.NOTICE.md

ifeq ($(shell uname -s),Darwin)
MACOS_SDK_PATH := $(shell xcrun --show-sdk-path 2>/dev/null)
ifneq ($(MACOS_SDK_PATH),)
CPPFLAGS += -isystem $(MACOS_SDK_PATH)/usr/include/c++/v1
endif
endif

CORE_SOURCES = src/system.cpp src/definition.cpp src/library.cpp src/penrose.cpp src/substitution.cpp src/view.cpp src/pdf.cpp src/discovery.cpp src/bank.cpp
CORE_OBJECTS = $(CORE_SOURCES:src/%.cpp=$(BUILD)/%.o)
APER_OBJECTS = $(BUILD)/main.o $(CORE_OBJECTS)
SEARCH_OBJECTS = $(BUILD)/search.o $(CORE_OBJECTS)
OBJECTS = $(APER_OBJECTS) $(SEARCH_OBJECTS) $(BUILD)/test.o $(BUILD)/test-definition.o
DEPS = $(sort $(OBJECTS:.o=.d))

.PHONY: all catalogue catalogue-offline check clean encyclopedia-bank rules-check
.PHONY: install install-rules install-encyclopedia

all: $(PROGRAMS)

rules-check: $(PROGRAM)
	@tools/check-rule-library --aper ./$(PROGRAM)

$(PROGRAM): $(APER_OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(SEARCH_PROGRAM): $(SEARCH_OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARNINGS) $(STANDARD) -Isrc -MMD -MP -c $< -o $@

$(BUILD)/test.o: tests/test.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARNINGS) $(STANDARD) -Isrc -MMD -MP -c $< -o $@

$(BUILD)/test-definition.o: tests/test_definition.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARNINGS) $(STANDARD) -Isrc -MMD -MP -c $< -o $@

$(BUILD)/aper-test: $(BUILD)/test.o $(CORE_OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/definition-test: $(BUILD)/test-definition.o $(CORE_OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD):
	mkdir -p $@

check: $(PROGRAMS) $(BUILD)/aper-test $(BUILD)/definition-test
	./$(BUILD)/aper-test
	./$(BUILD)/definition-test
	@tools/update-encyclopedia-bank --help >/dev/null
	@tools/fetch-encyclopedia-artwork --help >/dev/null
	@tools/build-catalogue --help >/dev/null
	@tools/check-rule-library --help >/dev/null
	@tools/reconstruct-lattice-rule --help >/dev/null
	@tools/check-rule-library --aper ./$(PROGRAM) >/dev/null
	@python3 tests/test_fetch_encyclopedia_artwork.py >/dev/null
	@python3 tests/test_reconstruct_lattice_rule.py >/dev/null
	@test "$$(awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" { count++ } END { print count }' $(ENCYCLOPEDIA_BANK))" -ge 250
	@test "$$(awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" { count++ } END { print count }' $(ENCYCLOPEDIA_BANK))" = \
		"$$(awk '$$1 == "#" && $$2 == "records:" { print $$3 }' $(ENCYCLOPEDIA_BANK))"
	@awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" && \
		(NF != 5 || $$3 != "https://tilings.math.uni-bielefeld.de/substitution/" $$1 "/") \
		{ exit 1 }' $(ENCYCLOPEDIA_BANK)
	@awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" { print $$1 }' \
		$(ENCYCLOPEDIA_BANK) | LC_ALL=C sort -c
	@test "$$(awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" && $$5 != "-" { count++ } END { print count }' $(ENCYCLOPEDIA_BANK))" -eq 15
	@test -z "$$(awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" { print $$1 }' \
		$(ENCYCLOPEDIA_BANK) | sort | uniq -d)"
	@./$(PROGRAM) -h | grep -q '^usage: aper'
	@./$(PROGRAM) -h | grep -q 'ammann-beenker: Ammann-Beenker'
	@./$(PROGRAM) -h | grep -q 'stampfli-12-fold-1'
	@./$(PROGRAM) -h | grep -q 'thue-morse-2d: Thue-Morse 2D'
	@./$(PROGRAM) -h | grep -q 'chair: Chair'
	@./$(PROGRAM) -h | grep -q 'domino: Domino'
	@./$(PROGRAM) -h | grep -q 'thin requires depth 2+'
	@./$(PROGRAM) -h | grep -q -- '--file FILE'
	@./$(PROGRAM) -h | grep -q -- '--rule'
	@./$(PROGRAM) -h | grep -q -- '--definition'
	@./$(PROGRAM) -h | grep -q -- '--library DIR'
	@./$(PROGRAM) -V | grep -q '^aper 0\.10\.0$$'
	@./$(PROGRAM) | grep -a -q '/Subject (sun seed at depth 7;'
	@./$(PROGRAM) -t p1 | grep -a -q '/Subject (pentagon-5 seed at depth 5;'
	@./$(PROGRAM) -t ammann-beenker | grep -a -q '/Subject (octagon seed at depth 4;'
	@./$(PROGRAM) -t pinwheel | grep -a -q '/Subject (triangle seed at depth 6;'
	@./$(PROGRAM) -t stampfli | grep -a -q '/Subject (dodecagon seed at depth 2;'
	@./$(PROGRAM) -t thue-morse-2d | grep -a -q '/Subject (a seed at depth 4;'
	@./$(PROGRAM) --file tests/data/square.aper -n 2 > $(BUILD)/aper-file.pdf
	@grep -a -q '/Title (Square file tiling - square)' $(BUILD)/aper-file.pdf
	@./$(PROGRAM) --file tests/data/square.aper --rule | \
		grep -a -q '/Title (Square file substitution rule)'
	@./$(PROGRAM) --file rules/chair.aper -n 2 | grep -a -q '%%EOF'
	@./$(PROGRAM) --file rules/domino.aper --rule | grep -a -q '%%EOF'
	@./$(PROGRAM) --file rules/thue-morse-2d.aper -n 2 | grep -a -q '%%EOF'
	@./$(PROGRAM) --library rules --tiling square-chair -n 2 | grep -a -q '%%EOF'
	@./$(PROGRAM) --tiling squiral-block --rule | grep -a -q '%%EOF'
	@./$(PROGRAM) --tiling pentomino -n 2 | grep -a -q '%%EOF'
	@./$(PROGRAM) --file rules/chair.aper --definition > $(BUILD)/chair-definition.aper
	@./$(PROGRAM) --file $(BUILD)/chair-definition.aper --definition > \
		$(BUILD)/chair-definition-second.aper
	@cmp $(BUILD)/chair-definition.aper $(BUILD)/chair-definition-second.aper
	@./$(PROGRAM) --tiling p2 --definition > $(BUILD)/p2-definition.aper
	@./$(PROGRAM) --file $(BUILD)/p2-definition.aper -n 1 | grep -a -q '%%EOF'
	@if ./$(PROGRAM) --tiling p1 --definition > $(BUILD)/p1-definition.aper 2>/dev/null; then \
		echo 'aper serialised a legacy system that aper 1 cannot represent' >&2; exit 1; \
	fi
	@test ! -s $(BUILD)/p1-definition.aper
	@./$(PROGRAM) --file - -n 2 < tests/data/square.aper > $(BUILD)/aper-stdin.pdf
	@cmp $(BUILD)/aper-file.pdf $(BUILD)/aper-stdin.pdf
	@if ./$(PROGRAM) -t p3 --file tests/data/square.aper >/dev/null 2>&1; then \
		echo 'aper accepted both a built-in and file system' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) --file tests/data/square.aper --library rules >/dev/null 2>&1; then \
		echo 'aper accepted both a file and rule library' >&2; exit 1; \
	fi
	@for tiling in p1 p2 p3 ammann-beenker pinwheel stampfli thue-morse-2d chair domino; do \
		./$(PROGRAM) -t $$tiling --rule | grep -a -q '/Title (.* substitution rule)' || exit 1; \
	done
	@./$(PROGRAM) -t p1 -r | grep -a -q '%%EOF'
	@./$(PROGRAM) -t p1 -n 1 | grep -a -q '%%EOF'
	@./$(PROGRAM) -t p1 -n 6 | grep -a -q '%%EOF'
	@./$(PROGRAM) -t p2 -n 1 | grep -a -q '%%EOF'
	@./$(PROGRAM) -t p3 -n 1 | grep -a -q '%%EOF'
	@for seed in pentagon-5 pentagon-3 pentagon-2 diamond boat star; do \
		./$(PROGRAM) -t p1 -s $$seed -n 1 | grep -a -q '%%EOF' || exit 1; \
	done
	@for seed in sun star ace deuce jack queen king; do \
		./$(PROGRAM) -t p2 -s $$seed -n 2 | grep -a -q '%%EOF' || exit 1; \
	done
	@for seed in sun star thin thick; do \
		./$(PROGRAM) -t p3 -s $$seed -n 2 | grep -a -q '%%EOF' || exit 1; \
	done
	@for seed in octagon square rhomb; do \
		./$(PROGRAM) -t ammann-beenker -s $$seed -n 2 | grep -a -q '%%EOF' || exit 1; \
	done
	@./$(PROGRAM) -t pinwheel -s triangle -n 3 | grep -a -q '%%EOF'
	@for seed in dodecagon triangle square rhomb; do \
		./$(PROGRAM) -t stampfli -s $$seed -n 1 | grep -a -q '%%EOF' || exit 1; \
	done
	@./$(PROGRAM) --tiling=p2 --seed=queen -n 2 | grep -a -q '%%EOF'
	@./$(PROGRAM) -t pentagon-boat-star -n 1 | grep -a -q '/Title (P1 '
	@./$(PROGRAM) -t kite-dart -n 1 | grep -a -q '/Title (P2 '
	@./$(PROGRAM) -t rhomb -n 1 | grep -a -q '/Title (P3 '
	@./$(PROGRAM) -t ab -n 1 | grep -a -q '/Title (Ammann-Beenker '
	@./$(PROGRAM) -t stampfli-12-fold-1 -n 1 | grep -a -q '/Title (Stampfli 12-fold 1 '
	@./$(PROGRAM) -t thue-morse -n 1 | grep -a -q '/Title (Thue-Morse 2D '
	@./$(PROGRAM) -t l-triomino -n 1 | grep -a -q '/Title (Chair '
	@./$(PROGRAM) -t table -n 1 | grep -a -q '/Title (Domino '
	@for scheme in flare grove electric tide; do \
		./$(PROGRAM) -c $$scheme -n 1 | grep -a -q '%%EOF' || exit 1; \
	done
	@./$(PROGRAM) --colour=tide -n 1 | grep -a -q '%%EOF'
	@if ./$(PROGRAM) -n 0 >/dev/null 2>&1; then \
		echo 'aper accepted an invalid depth' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) -t square >/dev/null 2>&1; then \
		echo 'aper accepted an invalid tiling' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) -c sepia >/dev/null 2>&1; then \
		echo 'aper accepted an invalid colour scheme' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) -s wheel >/dev/null 2>&1; then \
		echo 'aper accepted an invalid seed' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) -t p3 -s ace >/dev/null 2>&1; then \
		echo 'aper accepted a P2 seed for P3' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) -t p2 -s thin >/dev/null 2>&1; then \
		echo 'aper accepted a P3 seed for P2' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) -t p1 -s sun >/dev/null 2>&1; then \
		echo 'aper accepted a P2/P3 seed for P1' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) -t p2 -s pentagon-5 >/dev/null 2>&1; then \
		echo 'aper accepted a P1 seed for P2' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) -t p3 -s pentagon-5 >/dev/null 2>&1; then \
		echo 'aper accepted a P1 seed for P3' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) -t ammann-beenker -s triangle >/dev/null 2>&1; then \
		echo 'aper accepted a Pinwheel seed for Ammann-Beenker' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) -t pinwheel -s square >/dev/null 2>&1; then \
		echo 'aper accepted an Ammann-Beenker seed for Pinwheel' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) -t stampfli -s octagon >/dev/null 2>&1; then \
		echo 'aper accepted an Ammann-Beenker seed for Stampfli' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) -t p1 -n 7 >/dev/null 2>&1; then \
		echo 'aper accepted an excessive P1 depth' >&2; exit 1; \
	fi
	@./$(PROGRAM) -t p1 -n 13 2>&1 | grep -q 'P1 depth must be an integer from 1 to 6'
	@./$(PROGRAM) -t ammann-beenker -n 7 2>&1 | grep -q 'Ammann-Beenker depth must be an integer from 1 to 6'
	@./$(PROGRAM) -t pinwheel -n 9 2>&1 | grep -q 'Pinwheel depth must be an integer from 1 to 8'
	@./$(PROGRAM) -t stampfli -n 4 2>&1 | grep -q 'Stampfli 12-fold 1 depth must be an integer from 1 to 3'
	@./$(PROGRAM) -t thue-morse-2d -n 8 2>&1 | grep -q 'Thue-Morse 2D depth must be an integer from 1 to 7'
	@if ./$(PROGRAM) -t p3 -s thin -n 1 >/dev/null 2>&1; then \
		echo 'aper accepted an unrenderable thin seed depth' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) --rule --depth 2 >/dev/null 2>&1; then \
		echo 'aper accepted a patch-only option for rule output' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) --definition --depth 2 >/dev/null 2>&1; then \
		echo 'aper accepted a patch-only option for definition output' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) --definition --rule >/dev/null 2>&1; then \
		echo 'aper mixed definition and PDF output' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) --seed >/dev/null 2>&1; then \
		echo 'aper accepted a missing seed' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) --colour >/dev/null 2>&1; then \
		echo 'aper accepted a missing colour scheme' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) --tiling >/dev/null 2>&1; then \
		echo 'aper accepted a missing tiling' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) --depth >/dev/null 2>&1; then \
		echo 'aper accepted a missing depth' >&2; exit 1; \
	fi
	@./$(SEARCH_PROGRAM) -h | grep -q '^usage: aper-search'
	@./$(SEARCH_PROGRAM) -h | grep -q 'novelty or aperiodicity'
	@./$(SEARCH_PROGRAM) -h | grep -q -- '--definition'
	@./$(SEARCH_PROGRAM) -V | grep -q '^aper-search 0\.10\.0$$'
	@./$(SEARCH_PROGRAM) -n 2 > $(BUILD)/aper-search-first.pdf
	@grep -a -Fq '/Title (Chair \(L-triomino\) rep-tile tiling - chair)' $(BUILD)/aper-search-first.pdf
	@grep -a -q '%%EOF' $(BUILD)/aper-search-first.pdf
	@./$(SEARCH_PROGRAM) --depth=2 > $(BUILD)/aper-search-second.pdf
	@cmp $(BUILD)/aper-search-first.pdf $(BUILD)/aper-search-second.pdf
	@./$(SEARCH_PROGRAM) --rule > $(BUILD)/aper-search-rule.pdf
	@grep -a -Fq '/Title (Chair \(L-triomino\) rep-tile substitution rule)' $(BUILD)/aper-search-rule.pdf
	@grep -a -q '%%EOF' $(BUILD)/aper-search-rule.pdf
	@./$(SEARCH_PROGRAM) -r > $(BUILD)/aper-search-rule-second.pdf
	@cmp $(BUILD)/aper-search-rule.pdf $(BUILD)/aper-search-rule-second.pdf
	@./$(SEARCH_PROGRAM) --definition > $(BUILD)/aper-search-definition.aper
	@./$(PROGRAM) --file $(BUILD)/aper-search-definition.aper -n 2 > \
		$(BUILD)/aper-search-definition.pdf
	@cmp $(BUILD)/aper-search-first.pdf $(BUILD)/aper-search-definition.pdf
	@./$(PROGRAM) --file $(BUILD)/aper-search-definition.aper --rule > \
		$(BUILD)/aper-search-definition-rule.pdf
	@cmp $(BUILD)/aper-search-rule.pdf $(BUILD)/aper-search-definition-rule.pdf
	@./$(SEARCH_PROGRAM) -n 7 >/dev/null
	@./$(SEARCH_PROGRAM) -c tide -n 1 | grep -a -q '%%EOF'
	@if ./$(SEARCH_PROGRAM) -n 0 >/dev/null 2>&1; then \
		echo 'aper-search accepted an invalid depth' >&2; exit 1; \
	fi
	@if ./$(SEARCH_PROGRAM) -n 8 >/dev/null 2>&1; then \
		echo 'aper-search accepted an excessive depth' >&2; exit 1; \
	fi
	@if ./$(SEARCH_PROGRAM) -c sepia >/dev/null 2>&1; then \
		echo 'aper-search accepted an invalid colour scheme' >&2; exit 1; \
	fi
	@if ./$(SEARCH_PROGRAM) --rule --depth 2 >/dev/null 2>&1; then \
		echo 'aper-search accepted a patch-only option for rule output' >&2; exit 1; \
	fi
	@if ./$(SEARCH_PROGRAM) --definition --rule >/dev/null 2>&1; then \
		echo 'aper-search mixed definition and PDF output' >&2; exit 1; \
	fi
	@if ./$(SEARCH_PROGRAM) --space square --cells 3 >/dev/null 2>&1; then \
		echo 'aper-search accepted a polyomino option for square search' >&2; exit 1; \
	fi
	@if ./$(SEARCH_PROGRAM) --colour >/dev/null 2>&1; then \
		echo 'aper-search accepted a missing colour scheme' >&2; exit 1; \
	fi
	@if ./$(SEARCH_PROGRAM) --depth >/dev/null 2>&1; then \
		echo 'aper-search accepted a missing depth' >&2; exit 1; \
	fi
	@if ./$(SEARCH_PROGRAM) --bogus >/dev/null 2>&1; then \
		echo 'aper-search accepted an unknown option' >&2; exit 1; \
	fi
	@if ./$(SEARCH_PROGRAM) tile >/dev/null 2>&1; then \
		echo 'aper-search accepted an operand' >&2; exit 1; \
	fi
	@./$(SEARCH_PROGRAM) --list-known > $(BUILD)/aper-search-known.txt
	@test "$$(wc -l < $(BUILD)/aper-search-known.txt)" -eq 15
	@grep -q '^p3[[:space:]]penrose-rhomb[[:space:]]https://' $(BUILD)/aper-search-known.txt
	@grep -q '^thue-morse-2d[[:space:]]thue-morse-2d[[:space:]]https://' $(BUILD)/aper-search-known.txt
	@grep -q '^chair[[:space:]]chair[[:space:]]https://' $(BUILD)/aper-search-known.txt
	@grep -q '^domino[[:space:]]domino[[:space:]]https://' $(BUILD)/aper-search-known.txt
	@awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" && $$5 != "-" \
		{ print $$5 "\t" $$1 "\t" $$3 }' $(ENCYCLOPEDIA_BANK) | \
		LC_ALL=C sort > $(BUILD)/encyclopedia-known.txt
	@LC_ALL=C sort $(BUILD)/aper-search-known.txt | \
		cmp - $(BUILD)/encyclopedia-known.txt
	@./$(SEARCH_PROGRAM) --bank rules --list-known > \
		$(BUILD)/aper-search-explicit-known.txt
	@cmp $(BUILD)/aper-search-known.txt $(BUILD)/aper-search-explicit-known.txt
	@./$(SEARCH_PROGRAM) --classify > $(BUILD)/aper-search-classification.txt
	@grep -q '^exact-rule[[:space:]]chair[[:space:]]https://' $(BUILD)/aper-search-classification.txt
	@./$(SEARCH_PROGRAM) --space polyomino --cells 3 --list-candidates > \
		$(BUILD)/aper-search-polyomino-candidates.txt
	@grep -q '^# cells[[:space:]]3$$' $(BUILD)/aper-search-polyomino-candidates.txt
	@grep -q '^# generated[[:space:]]2$$' $(BUILD)/aper-search-polyomino-candidates.txt
	@grep -q '^# unique[[:space:]]2$$' $(BUILD)/aper-search-polyomino-candidates.txt
	@grep -q '^0[[:space:]]polyomino-chair[[:space:]]chair$$' \
		$(BUILD)/aper-search-polyomino-candidates.txt
	@grep -q '^1[[:space:]]polyomino-i[[:space:]]-$$' \
		$(BUILD)/aper-search-polyomino-candidates.txt
	@./$(SEARCH_PROGRAM) --space binary-square --list-candidates > \
		$(BUILD)/aper-search-binary-candidates.txt
	@grep -q '^# generated[[:space:]]256$$' $(BUILD)/aper-search-binary-candidates.txt
	@grep -q '^# algebraically-valid[[:space:]]224$$' $(BUILD)/aper-search-binary-candidates.txt
	@grep -q '^# geometrically-valid[[:space:]]224$$' $(BUILD)/aper-search-binary-candidates.txt
	@grep -q '^# unique[[:space:]]27$$' $(BUILD)/aper-search-binary-candidates.txt
	@test "$$(awk -F '\t' '$$1 ~ /^[0-9]+$$/ { count++ } END { print count }' \
		$(BUILD)/aper-search-binary-candidates.txt)" -eq 27
	@test "$$(awk -F '\t' '$$1 ~ /^[0-9]+$$/ && $$3 != "-" { count++ } END { print count }' \
		$(BUILD)/aper-search-binary-candidates.txt)" -eq 1
	@grep -q '^22[[:space:]]binary-square-06-09[[:space:]]thue-morse-2d$$' \
		$(BUILD)/aper-search-binary-candidates.txt
	@./$(SEARCH_PROGRAM) --space binary-square --candidate 22 -n 4 > \
		$(BUILD)/aper-search-binary-patch.pdf
	@grep -a -q '/Title (Binary square 06-09 tiling - a)' \
		$(BUILD)/aper-search-binary-patch.pdf
	@./$(SEARCH_PROGRAM) --space binary-square --candidate 22 --rule > \
		$(BUILD)/aper-search-binary-rule.pdf
	@grep -a -q '/Title (Binary square 06-09 substitution rule)' \
		$(BUILD)/aper-search-binary-rule.pdf
	@./$(SEARCH_PROGRAM) --space binary-square --candidate 22 --classify | \
		grep -q '^exact-rule[[:space:]]thue-morse-2d[[:space:]]https://'
	@./$(SEARCH_PROGRAM) --space binary-square --candidate 21 --classify | \
		grep -q '^no-exact-match[[:space:]]15 encoded systems checked$$'
	@if ./$(SEARCH_PROGRAM) --space binary-square --candidate 27 >/dev/null 2>&1; then \
		echo 'aper-search accepted an out-of-range candidate' >&2; exit 1; \
	fi
	@if ./$(SEARCH_PROGRAM) --list-candidates --candidate 0 >/dev/null 2>&1; then \
		echo 'aper-search mixed candidate listing and selection' >&2; exit 1; \
	fi
	@if ./$(SEARCH_PROGRAM) --classify --rule >/dev/null 2>&1; then \
		echo 'aper-search mixed classification and PDF output' >&2; exit 1; \
	fi

encyclopedia-bank:
	tools/update-encyclopedia-bank

catalogue: $(PROGRAMS)
	tools/fetch-encyclopedia-artwork
	tools/build-catalogue

catalogue-offline: $(PROGRAMS)
	tools/fetch-encyclopedia-artwork --offline
	tools/build-catalogue

install: $(PROGRAMS) install-rules
	install -d "$(DESTDIR)$(PREFIX)/bin" "$(DESTDIR)$(PREFIX)/share/man/man1"
	install -m 0755 $(PROGRAM) "$(DESTDIR)$(PREFIX)/bin/$(PROGRAM)"
	install -m 0755 $(SEARCH_PROGRAM) "$(DESTDIR)$(PREFIX)/bin/$(SEARCH_PROGRAM)"
	install -m 0644 man/aper.1 "$(DESTDIR)$(PREFIX)/share/man/man1/aper.1"
	install -m 0644 man/aper-search.1 "$(DESTDIR)$(PREFIX)/share/man/man1/aper-search.1"

install-rules: $(RULE_FILES)
	install -d "$(DESTDIR)$(RULE_INSTALL_DIR)"
	install -m 0644 $(RULE_FILES) "$(DESTDIR)$(RULE_INSTALL_DIR)"

install-encyclopedia: $(ENCYCLOPEDIA_BANK) $(ENCYCLOPEDIA_NOTICE)
	install -d "$(DESTDIR)$(PREFIX)/share/aper"
	install -m 0644 $(ENCYCLOPEDIA_BANK) \
		"$(DESTDIR)$(PREFIX)/share/aper/encyclopedia.tsv"
	install -m 0644 $(ENCYCLOPEDIA_NOTICE) \
		"$(DESTDIR)$(PREFIX)/share/aper/encyclopedia.NOTICE.md"

clean:
	rm -f $(PROGRAMS) $(BUILD)/*.o $(BUILD)/*.d $(BUILD)/aper-test \
		$(BUILD)/definition-test
	rm -f $(BUILD)/*.pdf $(BUILD)/*.png $(BUILD)/*.txt $(BUILD)/*.tmp
	rm -f $(BUILD)/*.aper
	rm -rf $(BUILD)/catalogue $(BUILD)/catalogue-smoke
	rmdir $(BUILD) 2>/dev/null || true

-include $(DEPS)
