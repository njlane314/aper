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
	@./$(PROGRAM) -V | grep -q '^aper 0\.2\.0$$'
	@./$(PROGRAM) -t p2 -n 1 | grep -a -q '%%EOF'
	@./$(PROGRAM) -t p3 -n 1 | grep -a -q '%%EOF'
	@if ./$(PROGRAM) -n 0 >/dev/null 2>&1; then \
		echo 'aper accepted an invalid depth' >&2; exit 1; \
	fi
	@if ./$(PROGRAM) -t square >/dev/null 2>&1; then \
		echo 'aper accepted an invalid tiling' >&2; exit 1; \
	fi

install: $(PROGRAM)
	install -d "$(DESTDIR)$(PREFIX)/bin" "$(DESTDIR)$(PREFIX)/share/man/man1"
	install -m 0755 $(PROGRAM) "$(DESTDIR)$(PREFIX)/bin/$(PROGRAM)"
	install -m 0644 man/aper.1 "$(DESTDIR)$(PREFIX)/share/man/man1/aper.1"

clean:
	rm -f $(PROGRAM) $(BUILD)/*.o $(BUILD)/*.d $(BUILD)/aper-test
	rmdir $(BUILD) 2>/dev/null || true

-include $(DEPS)
