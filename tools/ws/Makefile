# ws — the AI Workspace command-line tool. C++20, POSIX, SQLite.
CXX      ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Ibuild/include -Isrc
LDLIBS   += -lsqlite3

JSON_VERSION := 3.11.3
JSON_SHA256  := 9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6
JSON_URL     := https://github.com/nlohmann/json/releases/download/v$(JSON_VERSION)/json.hpp
JSON_HDR     := build/include/nlohmann/json.hpp

SRCS := $(shell find src -name '*.cpp' | sort)
OBJS := $(SRCS:src/%.cpp=build/obj/%.o)
DEPS := $(OBJS:.o=.d)

all: build/ws

build/ws: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/obj/%.o: src/%.cpp $(JSON_HDR)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Third-party header: downloaded at build time, never committed; checksum-pinned.
$(JSON_HDR):
	@mkdir -p $(dir $@)
	curl -sfL -o $@.tmp $(JSON_URL)
	@actual=$$( (command -v sha256sum >/dev/null && sha256sum $@.tmp || shasum -a 256 $@.tmp) | cut -d' ' -f1 ); \
	if [ "$$actual" != "$(JSON_SHA256)" ]; then echo "checksum mismatch for nlohmann/json: $$actual" >&2; rm -f $@.tmp; exit 1; fi
	mv $@.tmp $@

test: build/ws
	cd test && node --test --test-reporter=spec "**/*.test.ts"

clean:
	rm -rf build/obj build/ws

.PHONY: all test clean
-include $(DEPS)
