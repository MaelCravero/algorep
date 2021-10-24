CXX = mpic++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++20
CPPFLAGS = -Isrc -MMD

SRC = src/main.cc \
      src/client.cc \
      src/server.cc \
      src/utils/logger.cc \
      src/utils/log_entries.cc

OBJ = $(SRC:.cc=.o)

DEP = ${SRC:.cc=.d}

BIN = algorep

.PHONY: all run clean

NP ?= 16
HOSTFILE ?= hostfile

all: run

run: $(BIN)
	mpirun -np $(NP) -hostfile $(HOSTFILE) $(BIN)

$(BIN): $(OBJ)
	$(CXX) -o $@ $^

-include ${DEP}

clean:
	$(RM) $(OBJ) $(BIN) $(DEP)
