CXX = mpic++
CXXFLAGS = -std=c++17 -O2 -ffp-contract=off

all: heat_blocks

heat_blocks: src/blocks.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f heat_blocks

.PHONY: all clean
