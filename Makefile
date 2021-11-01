CXX = mpic++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++20 -g
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
CMD_FILE ?= commands.txt
NCMD ?= 5

all: run

run: $(BIN)
	mpirun -np $$(($(NSERVER) + $(NCLIENT) + 1)) -hostfile $(HOSTFILE) $(BIN) $(NSERVER) $(NCLIENT)

$(BIN): $(OBJ)
	$(CXX) -o $@ $^

debug: CPPFLAGS += -D_DEBUG
debug: clean run

perf: CPPFLAGS += -D_PERF
perf: clean run

gen_commands:
	./gen_cmd.sh $(NSERVER) $(NCLIENT) $(CMD_FILE) $(NCMD)

-include ${DEP}

clean:
	$(RM) $(OBJ) $(BIN) $(DEP) *.log *.csv
