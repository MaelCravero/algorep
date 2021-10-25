CXX = mpic++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++20
CPPFLAGS = -Isrc -MMD

SRC = src/main.cc \
      src/client.cc \
      src/server.cc \
      src/repl.cc \
      src/utils/logger.cc \
      src/utils/log_entries.cc

OBJ = $(SRC:.cc=.o)

DEP = ${SRC:.cc=.d}

BIN = algorep

.PHONY: all run clean

NSERVER ?= 5
NCLIENT ?= 5
HOSTFILE ?= hostfile

all: run

run: $(BIN)
	mpirun -np $$(($(NSERVER) + $(NCLIENT) + 1)) -hostfile $(HOSTFILE) $(BIN) $(NSERVER) $(NCLIENT)

$(BIN): $(OBJ)
	$(CXX) -o $@ $^

-include ${DEP}

clean:
	$(RM) $(OBJ) $(BIN) $(DEP) *.log
