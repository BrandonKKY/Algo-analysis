# Greedy seam carving (C++17)
#   make                                         build build/greedy_carve
#   make greedy IMG=images/castle.jpg WIDTH=400 HEIGHT=300
#   make greedy-multistart IMG=... WIDTH=...      greedy from every top pixel
#   make test                                    build + run unit tests
#   make clean                                   delete build/ and output/

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CPPFLAGS += -Isrc -Ithird_party

ifeq ($(OS),Windows_NT)
  EXE := .exe
endif

IMG    ?= images/input.jpg
WIDTH  ?=
HEIGHT ?=
OUT    ?= output/greedy
SIZE_ARGS = $(if $(WIDTH),--width $(WIDTH)) $(if $(HEIGHT),--height $(HEIGHT))

LIB_SRC  := src/image.cpp src/energy.cpp src/greedy.cpp
LIB_OBJ  := $(LIB_SRC:src/%.cpp=build/%.o)
BIN      := build/greedy_carve$(EXE)
TEST_BIN := build/test_greedy$(EXE)

.PHONY: all greedy greedy-multistart test clean

all: $(BIN)

$(BIN): $(LIB_OBJ) build/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TEST_BIN): $(LIB_OBJ) build/test_greedy.o
	$(CXX) $(CXXFLAGS) $^ -o $@

build/%.o: src/%.cpp src/*.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/test_greedy.o: tests/test_greedy.cpp src/*.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build:
	mkdir -p build

greedy: $(BIN)
	./$(BIN) "$(IMG)" $(SIZE_ARGS) --mode min --out-dir "$(OUT)"

greedy-multistart: $(BIN)
	./$(BIN) "$(IMG)" $(SIZE_ARGS) --mode multistart --out-dir "$(OUT)"

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf build output
