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

ifeq ($(shell uname -s),Darwin)
MACOS_SDK_PATH := $(shell xcrun --show-sdk-path 2>/dev/null)
ifneq ($(MACOS_SDK_PATH),)
CPPFLAGS += -isystem $(MACOS_SDK_PATH)/usr/include/c++/v1
endif
endif

SOURCES = src/main.cpp src/system.cpp src/penrose.cpp src/substitution.cpp src/view.cpp src/pdf.cpp
OBJECTS = $(SOURCES:src/%.cpp=$(BUILD)/%.o)
DEPS = $(OBJECTS:.o=.d) $(BUILD)/test.d

.PHONY: all check clean install

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARNINGS) $(STANDARD) -Isrc -MMD -MP -c $< -o $@

$(BUILD)/test.o: tests/test.cpp | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARNINGS) $(STANDARD) -Isrc -MMD -MP -c $< -o $@

$(BUILD)/aper-test: $(BUILD)/test.o $(BUILD)/system.o $(BUILD)/penrose.o $(BUILD)/substitution.o $(BUILD)/view.o $(BUILD)/pdf.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD):
	mkdir -p $@

check: $(PROGRAM) $(BUILD)/aper-test
	./$(BUILD)/aper-test
	@./$(PROGRAM) -h | grep -q '^usage: aper'
	@./$(PROGRAM) -h | grep -q 'ammann-beenker: Ammann-Beenker'
	@./$(PROGRAM) -h | grep -q 'stampfli-12-fold-1'
	@./$(PROGRAM) -h | grep -q 'thin requires depth 2+'
	@./$(PROGRAM) -h | grep -q -- '--rule'
	@./$(PROGRAM) -V | grep -q '^aper 0\.7\.0$$'
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

install: $(PROGRAM)
	install -d "$(DESTDIR)$(PREFIX)/bin" "$(DESTDIR)$(PREFIX)/share/man/man1"
	install -m 0755 $(PROGRAM) "$(DESTDIR)$(PREFIX)/bin/$(PROGRAM)"
	install -m 0644 man/aper.1 "$(DESTDIR)$(PREFIX)/share/man/man1/aper.1"

clean:
	rm -f $(PROGRAM) $(BUILD)/*.o $(BUILD)/*.d $(BUILD)/aper-test
	rmdir $(BUILD) 2>/dev/null || true

-include $(DEPS)
