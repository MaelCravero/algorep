#pragma once

#include <mpi.h>

#include "common.hh"
#include "utils/time.hh"

class Client
{
public:
    struct ClientMessage : public Message
    {
        int request_id;
        int entry;
    };

    Client(rank rank, int nb_server);

    bool send_request();
    bool started();
    bool recv_order();

private:
    rank rank_;
    int nb_server_;
    int server_;

    int request_id_;
    bool started_;
};
