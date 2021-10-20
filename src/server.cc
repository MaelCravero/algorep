#include "server.hh"

void server(int rank, int size)
{
    Server server(rank, size);

    // TODO
}

Server::Server(int rank, int size)
    : rank_(rank)
    , size_(size)
{}
