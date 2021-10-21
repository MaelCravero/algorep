#pragma once

#include <mpi.h>

#include "common.hh"

class Client
{
public:
    Client(rank rank, int size);

    void send_request();

private:
    rank rank_;
    int size_;
};
