#include <iostream>
#include <mpi.h>

#include "client.hh"
#include "server.hh"

void server(rank rank, int nb_server)
{
    Server server(rank, nb_server);

    while (true)
    {
        server.update();
    }
}

void client(rank rank, int nb_server)
{
    Client client(rank, nb_server);

    while (!client.send_request())
        continue;
}

bool is_client(int rank, int nb_server)
{
    return rank >= nb_server;
}

int main(int argc, char* argv[])
{
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // TODO: add a real argument parser ?
    if (argc != 3)
    {
        if (!rank)
            std::cout << "usage: " << argv[0] << " nb_server nb_client\n";
        return 1;
    }

    int nb_server = std::stoi(argv[1]);
    int nb_client = std::stoi(argv[2]);

    if (!rank)
    {
        std::cout << "nb_server: " << nb_server << " nb_client: " << nb_client
                  << "\n";
    }

    if (is_client(rank, nb_server))
        client(rank, nb_server);

    else
        server(rank, nb_server);

    MPI_Finalize();

    return 0;
}
