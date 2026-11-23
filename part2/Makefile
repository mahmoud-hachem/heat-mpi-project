CXX = mpic++
CXXFLAGS = -std=c++17 -O2 -ffp-contract=off

all: heat_strips

heat_strips: src/strips.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f heat_strips

.PHONY: all clean
