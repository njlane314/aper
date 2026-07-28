CXX ?= c++
CPPFLAGS ?=
CXXFLAGS ?= -O2
LDFLAGS ?=
LDLIBS ?=

PREFIX ?= /usr/local
DESTDIR ?=

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

CORE_SOURCES = src/system.cpp src/penrose.cpp src/substitution.cpp src/view.cpp src/pdf.cpp src/discovery.cpp src/bank.cpp
CORE_OBJECTS = $(CORE_SOURCES:src/%.cpp=$(BUILD)/%.o)
APER_OBJECTS = $(BUILD)/main.o $(CORE_OBJECTS)
SEARCH_OBJECTS = $(BUILD)/search.o $(CORE_OBJECTS)
OBJECTS = $(APER_OBJECTS) $(SEARCH_OBJECTS) $(BUILD)/test.o
DEPS = $(sort $(OBJECTS:.o=.d))

.PHONY: all check clean encyclopedia-bank install install-encyclopedia

all: $(PROGRAMS)

$(PROGRAM): $(APER_OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(SEARCH_PROGRAM): $(SEARCH_OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARNINGS) $(STANDARD) -Isrc -MMD -MP -c $< -o $@

$(BUILD)/test.o: tests/test.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARNINGS) $(STANDARD) -Isrc -MMD -MP -c $< -o $@

$(BUILD)/aper-test: $(BUILD)/test.o $(CORE_OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD):
	mkdir -p $@

check: $(PROGRAMS) $(BUILD)/aper-test
	./$(BUILD)/aper-test
	@tools/update-encyclopedia-bank --help >/dev/null
	@test "$$(awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" { count++ } END { print count }' $(ENCYCLOPEDIA_BANK))" -ge 250
	@test "$$(awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" { count++ } END { print count }' $(ENCYCLOPEDIA_BANK))" = \
		"$$(awk '$$1 == "#" && $$2 == "records:" { print $$3 }' $(ENCYCLOPEDIA_BANK))"
	@awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" && \
		(NF != 5 || $$3 != "https://tilings.math.uni-bielefeld.de/substitution/" $$1 "/") \
		{ exit 1 }' $(ENCYCLOPEDIA_BANK)
	@awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" { print $$1 }' \
		$(ENCYCLOPEDIA_BANK) | LC_ALL=C sort -c
	@test "$$(awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" && $$5 != "-" { count++ } END { print count }' $(ENCYCLOPEDIA_BANK))" -eq 6
	@test -z "$$(awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" { print $$1 }' \
		$(ENCYCLOPEDIA_BANK) | sort | uniq -d)"
	@./$(PROGRAM) -h | grep -q '^usage: aper'
	@./$(PROGRAM) -h | grep -q 'ammann-beenker: Ammann-Beenker'
	@./$(PROGRAM) -h | grep -q 'stampfli-12-fold-1'
	@./$(PROGRAM) -h | grep -q 'thin requires depth 2+'
	@./$(PROGRAM) -h | grep -q -- '--rule'
	@./$(PROGRAM) -V | grep -q '^aper 0\.8\.0$$'
	@./$(PROGRAM) | grep -a -q '/Subject (sun seed at depth 7;'
	@./$(PROGRAM) -t p1 | grep -a -q '/Subject (pentagon-5 seed at depth 5;'
	@./$(PROGRAM) -t ammann-beenker | grep -a -q '/Subject (octagon seed at depth 4;'
	@./$(PROGRAM) -t pinwheel | grep -a -q '/Subject (triangle seed at depth 6;'
	@./$(PROGRAM) -t stampfli | grep -a -q '/Subject (dodecagon seed at depth 2;'
	@for tiling in p1 p2 p3 ammann-beenker pinwheel stampfli; do \
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
	@if ./$(PROGRAM) -t p3 -s thin -n 1 >/dev/null 2>&1; then \
		echo 'aper accepted an unrenderable thin seed depth' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) --rule --depth 2 >/dev/null 2>&1; then \
		echo 'aper accepted a patch-only option for rule output' >&2; exit 1; \
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
	@./$(SEARCH_PROGRAM) -h | grep -q 'does not claim novelty or aperiodicity'
	@./$(SEARCH_PROGRAM) -V | grep -q '^aper-search 0\.8\.0$$'
	@./$(SEARCH_PROGRAM) -n 2 > $(BUILD)/aper-search-first.pdf
	@grep -a -q '/Title (Square 2x2 control tiling - square)' $(BUILD)/aper-search-first.pdf
	@grep -a -q '%%EOF' $(BUILD)/aper-search-first.pdf
	@./$(SEARCH_PROGRAM) --depth=2 > $(BUILD)/aper-search-second.pdf
	@cmp $(BUILD)/aper-search-first.pdf $(BUILD)/aper-search-second.pdf
	@./$(SEARCH_PROGRAM) --rule > $(BUILD)/aper-search-rule.pdf
	@grep -a -q '/Title (Square 2x2 control substitution rule)' $(BUILD)/aper-search-rule.pdf
	@grep -a -q '%%EOF' $(BUILD)/aper-search-rule.pdf
	@./$(SEARCH_PROGRAM) -r > $(BUILD)/aper-search-rule-second.pdf
	@cmp $(BUILD)/aper-search-rule.pdf $(BUILD)/aper-search-rule-second.pdf
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
	@test "$$(wc -l < $(BUILD)/aper-search-known.txt)" -eq 6
	@grep -q '^p3[[:space:]]penrose-rhomb[[:space:]]https://' $(BUILD)/aper-search-known.txt
	@awk -F '\t' '$$1 !~ /^#/ && $$1 != "slug" && $$5 != "-" \
		{ print $$5 "\t" $$1 "\t" $$3 }' $(ENCYCLOPEDIA_BANK) | \
		LC_ALL=C sort > $(BUILD)/encyclopedia-known.txt
	@LC_ALL=C sort $(BUILD)/aper-search-known.txt | \
		cmp - $(BUILD)/encyclopedia-known.txt
	@./$(SEARCH_PROGRAM) --classify > $(BUILD)/aper-search-classification.txt
	@grep -q '^no-exact-match[[:space:]]6 encoded systems checked$$' $(BUILD)/aper-search-classification.txt
	@if ./$(SEARCH_PROGRAM) --classify --rule >/dev/null 2>&1; then \
		echo 'aper-search mixed classification and PDF output' >&2; exit 1; \
	fi

encyclopedia-bank:
	tools/update-encyclopedia-bank

install: $(PROGRAMS)
	install -d "$(DESTDIR)$(PREFIX)/bin" "$(DESTDIR)$(PREFIX)/share/man/man1"
	install -m 0755 $(PROGRAM) "$(DESTDIR)$(PREFIX)/bin/$(PROGRAM)"
	install -m 0755 $(SEARCH_PROGRAM) "$(DESTDIR)$(PREFIX)/bin/$(SEARCH_PROGRAM)"
	install -m 0644 man/aper.1 "$(DESTDIR)$(PREFIX)/share/man/man1/aper.1"
	install -m 0644 man/aper-search.1 "$(DESTDIR)$(PREFIX)/share/man/man1/aper-search.1"

install-encyclopedia: $(ENCYCLOPEDIA_BANK) $(ENCYCLOPEDIA_NOTICE)
	install -d "$(DESTDIR)$(PREFIX)/share/aper"
	install -m 0644 $(ENCYCLOPEDIA_BANK) \
		"$(DESTDIR)$(PREFIX)/share/aper/encyclopedia.tsv"
	install -m 0644 $(ENCYCLOPEDIA_NOTICE) \
		"$(DESTDIR)$(PREFIX)/share/aper/encyclopedia.NOTICE.md"

clean:
	rm -f $(PROGRAMS) $(BUILD)/*.o $(BUILD)/*.d $(BUILD)/aper-test
	rm -f $(BUILD)/*.pdf $(BUILD)/*.png $(BUILD)/*.txt $(BUILD)/*.tmp
	rmdir $(BUILD) 2>/dev/null || true

-include $(DEPS)
