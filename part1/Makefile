CXX = g++
CXXFLAGS = -std=c++17 -O2 -ffp-contract=off

all: heat

heat: src/serial.cpp
	$(CXX) $(CXXFLAGS) src/serial.cpp -o heat

# Convenient alias for building the serial implementation.
serial: heat

clean:
	rm -f heat

.PHONY: all serial clean
