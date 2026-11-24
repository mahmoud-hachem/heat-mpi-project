CXX = g++
CXXFLAGS = -std=c++17 -O2 -ffp-contract=off

all: heat

heat: src/serial.cpp
	$(CXX) $(CXXFLAGS) src/serial.cpp -o heat

unstable_demo: src/serial_unstable_demo.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

# Convenient alias for building the serial implementation.
serial: heat

clean:
	rm -f heat unstable_demo

.PHONY: all serial clean
