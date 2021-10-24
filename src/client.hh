#pragma once

#include <mpi.h>

#include "common.hh"

class Client
{
public:
    Client(rank rank, int nb_server);

    bool send_request();

private:
    rank rank_;
    int nb_server_;
    int server_;
};
