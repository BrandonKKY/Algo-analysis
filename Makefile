CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
SRC      := $(wildcard src/*.cpp)

seamcarve: $(SRC) $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

run: seamcarve
	./seamcarve 8080 .

# Checks graph cut against the DP reference and prints a timing table.
test_graphcut: tests/test_graphcut.cpp $(filter-out src/main.cpp src/server.cpp,$(SRC)) $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) -o $@ tests/test_graphcut.cpp $(filter-out src/main.cpp src/server.cpp,$(SRC))

test: test_graphcut
	./test_graphcut

clean:
	rm -f seamcarve test_graphcut
