.PHONY: all clean debug release remake

EXE=solve
SRC=solver.cpp
CC=g++
CXXFLAGS=-std=c++17 -Wall

all: $(EXE)

debug: CXXFLAGS += -O0 -DDEBUG -g -pedantic
debug: $(EXE)

release: CXXFLAGS += -O3 -DNDEBUG
release: $(EXE)

$(EXE): $(SRC) Makefile
	$(CC) $(CXXFLAGS) $(SRC) -o $(EXE)

clean:
	rm -f $(EXE)

remake: clean all

