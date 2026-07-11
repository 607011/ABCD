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
release: strip

$(EXE): $(SRC) Makefile
	$(CC) $(CXXFLAGS) $(SRC) -o $(EXE)

strip: $(EXE)
	strip $(EXE)

clean:
	rm -f $(EXE)

remake: clean all

