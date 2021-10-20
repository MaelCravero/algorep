#include <iostream>
#include <mpi.h>

#include "client.hh"
#include "server.hh"

bool is_client(int rank)
{
    return rank % 2;
}

int main(int argc, char* argv[])
{
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (is_client(rank))
        client(rank, size);
    else
        server(rank, size);

    MPI_Finalize();

    return 0;
}
