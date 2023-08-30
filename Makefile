CXX?=g++
CXXFLAGS?=-g -Wall -Wextra

tests: sufftree.o tests.o
	$(CXX) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

all: tests
