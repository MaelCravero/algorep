#pragma once

#include <mpi.h>

class Client
{
public:
    Client(int rank, int size);

    void send_request();

private:
    int rank_;
    int size_;
};

void client(Client client);
