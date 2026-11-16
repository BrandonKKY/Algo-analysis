# Seam carving (C++17)
#   make                                         build everything
#   make run                                     start the web UI on http://localhost:8080
#   make greedy IMG=images/castle.jpg WIDTH=400 HEIGHT=300
#   make greedy-multistart IMG=... WIDTH=...      greedy from every top pixel
#   make test                                    build + run all unit tests
#   make clean                                   delete build/, output/ and seamcarve

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CPPFLAGS += -Isrc -isystem third_party

ifeq ($(OS),Windows_NT)
  EXE := .exe
endif

IMG    ?= images/input.jpg
WIDTH  ?=
HEIGHT ?=
OUT    ?= output/greedy
SIZE_ARGS = $(if $(WIDTH),--width $(WIDTH)) $(if $(HEIGHT),--height $(HEIGHT))

# Web UI + seam-finder interface (original / dp / greedy / graphcut).
# The server uses POSIX sockets: Linux, macOS or WSL only.
UI_SRC   := src/main.cpp src/server.cpp src/seam_carving.cpp src/seam_methods.cpp src/image.cpp src/energy.cpp \
            src/seam_dp.cpp src/seam_greedy.cpp src/seam_graphcut.cpp
UI_HDR   := $(wildcard src/*.h src/*.hpp)
SEAM_SRC := $(filter-out src/main.cpp src/server.cpp,$(UI_SRC))

# Greedy command-line tool.
LIB_SRC  := src/image.cpp src/energy.cpp src/greedy.cpp
LIB_OBJ  := $(LIB_SRC:src/%.cpp=build/%.o)
BIN      := build/greedy_carve$(EXE)
TEST_BIN := build/test_greedy$(EXE)
GC_TEST  := build/test_graphcut$(EXE)
DP_TEST  := build/test_dynamicprogramming$(EXE)

.PHONY: all run greedy greedy-multistart test clean

ifeq ($(OS),Windows_NT)
all: $(BIN)
else
all: seamcarve $(BIN)
endif

seamcarve: $(UI_SRC) $(UI_HDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -pthread -o $@ $(UI_SRC)

run: seamcarve
	./seamcarve 8080 .

$(BIN): $(LIB_OBJ) build/greedy_main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TEST_BIN): $(LIB_OBJ) build/test_greedy.o
	$(CXX) $(CXXFLAGS) $^ -o $@

# Checks graph cut against dynamic programming and prints a timing table.
$(GC_TEST): tests/test_graphcut.cpp $(SEAM_SRC) $(UI_HDR) | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ tests/test_graphcut.cpp $(SEAM_SRC)

# Checks dynamic programming against brute force, the reference DP and greedy.
$(DP_TEST): tests/test_dynamicprogramming.cpp $(SEAM_SRC) $(UI_HDR) | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ tests/test_dynamicprogramming.cpp $(SEAM_SRC)

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

test: $(TEST_BIN) $(GC_TEST) $(DP_TEST)
	./$(TEST_BIN)
	./$(GC_TEST)
	./$(DP_TEST)

clean:
	rm -rf build output
	rm -f seamcarve
