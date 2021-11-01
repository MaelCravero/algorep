#pragma once

#include <mpi.h>
#include <string>
#include <vector>

#include "common.hh"
#include "utils/time.hh"

class Client
{
public:
    using command_list = std::vector<std::string>;

    Client(rank rank, int nb_server, std::string cmd_file);

    bool done() const;
    void send_request();
    bool started();
    bool recv_order();

private:
    rank rank_;
    int nb_server_;
    int server_;

    unsigned request_id_;
    bool started_;
    bool done_;

    command_list command_list_;
};
