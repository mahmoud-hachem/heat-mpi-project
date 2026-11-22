MPICXX ?= mpic++
CXXFLAGS ?= -std=c++17 -O2 -ffp-contract=off -Wall -Wextra

.PHONY: all clean
all: bin/hello bin/pingpong

bin:
	mkdir -p bin

bin/hello: src/hello.cpp | bin
	$(MPICXX) $(CXXFLAGS) $< -o $@

bin/pingpong: src/pingpong.cpp | bin
	$(MPICXX) $(CXXFLAGS) $< -o $@

clean:
	rm -rf bin
