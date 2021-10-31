#pragma once

#include <mpi.h>
#include <string>
#include <vector>

#include "common.hh"
#include "mpi/mpi.hh"
#include "utils/time.hh"

class Client
{
public:
    using command_list = std::vector<std::string>;

    Client(rank rank, int nb_server, std::string cmd_file);

    bool done() const;
    bool send_request();
    bool started();
    bool recv_order();

private:
    rank rank_;
    int nb_server_;
    int server_;

    unsigned request_id_;
    bool started_;

    command_list command_list_;
    mpi::Mpi mpi_;
};
