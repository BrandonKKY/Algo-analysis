CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
SRC      := $(wildcard src/*.cpp)

seamcarve: $(SRC) $(wildcard src/*.h)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

run: seamcarve
	./seamcarve 8080 .

clean:
	rm -f seamcarve
