#pragma once

#include <optional>
#include <string>

#include "common.hh"
#include "mpi/mpi.hh"
#include "rpc/rpc.hh"

class Repl
{
public:
    using Order = rpc::Repl::Order;

private:
    struct Command
    {
        Order order;
        int target;
        int speed_level;
    };

public:
    Repl(int nb_server, int nb_client);

    std::optional<Command> parse_command(std::string line);
    void execute(Command command);
    void operator()();

private:
    int nb_server_;
    int nb_client_;
    mpi::Mpi mpi_;
};
