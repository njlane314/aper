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

SOURCES = src/main.cpp src/penrose.cpp src/pdf.cpp
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

$(BUILD)/aper-test: $(BUILD)/test.o $(BUILD)/penrose.o $(BUILD)/pdf.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD):
	mkdir -p $@

check: $(PROGRAM) $(BUILD)/aper-test
	./$(BUILD)/aper-test
	@./$(PROGRAM) -h | grep -q '^usage: aper'
	@./$(PROGRAM) -V | grep -q '^aper 0\.4\.0$$'
	@./$(PROGRAM) -t p2 -n 1 | grep -a -q '%%EOF'
	@./$(PROGRAM) -t p3 -n 1 | grep -a -q '%%EOF'
	@for seed in sun star ace deuce jack queen king; do \
		./$(PROGRAM) -t p2 -s $$seed -n 2 | grep -a -q '%%EOF' || exit 1; \
	done
	@for seed in sun star thin thick; do \
		./$(PROGRAM) -t p3 -s $$seed -n 2 | grep -a -q '%%EOF' || exit 1; \
	done
	@./$(PROGRAM) --tiling=p2 --seed=queen -n 2 | grep -a -q '%%EOF'
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
	@if ./$(PROGRAM) -t p3 -s thin -n 1 >/dev/null 2>&1; then \
		echo 'aper accepted an unrenderable thin seed depth' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) --seed >/dev/null 2>&1; then \
		echo 'aper accepted a missing seed' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) --colour >/dev/null 2>&1; then \
		echo 'aper accepted a missing colour scheme' >&2; exit 1; \
	fi

install: $(PROGRAM)
	install -d "$(DESTDIR)$(PREFIX)/bin" "$(DESTDIR)$(PREFIX)/share/man/man1"
	install -m 0755 $(PROGRAM) "$(DESTDIR)$(PREFIX)/bin/$(PROGRAM)"
	install -m 0644 man/aper.1 "$(DESTDIR)$(PREFIX)/share/man/man1/aper.1"

clean:
	rm -f $(PROGRAM) $(BUILD)/*.o $(BUILD)/*.d $(BUILD)/aper-test
	rmdir $(BUILD) 2>/dev/null || true

-include $(DEPS)
