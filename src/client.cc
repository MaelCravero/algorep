#include "client.hh"

Client::Client(int rank, int size)
    : rank_(rank)
    , size_(size)
{}

void Client::send_request()
{
    MPI_Send(&rank_, 1, MPI_INT, (rank_ + 1) % size_, 0, MPI_COMM_WORLD);
}
