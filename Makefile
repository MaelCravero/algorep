CXX = mpic++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++20
CPPFLAGS = -Isrc

SRC = src/main.cc \
      src/client.cc \
      src/server.cc

OBJ = $(SRC:.cc=.o)

BIN = algorep

.PHONY: all run clean

NP ?= 16
HOSTFILE ?= hostfile

all: run

run: $(BIN)
	mpirun -np $(NP) -hostfile $(HOSTFILE) $(BIN)

$(BIN): $(OBJ)
	$(CXX) -o $@ $^ 

clean:
	$(RM) $(OBJ) $(BIN)
