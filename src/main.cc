#include <iostream>
#include <mpi.h>

#include "client.hh"
#include "repl.hh"
#include "server.hh"

void server(rank rank, int nb_server, int nb_client)
{
    Server server(rank, nb_server);

    while (server.get_log_number() != nb_client)
        server.update();
}

void client(rank rank, int nb_server)
{
    Client client(rank, nb_server);

    while (!client.send_request())
        continue;
}

void repl(int nb_server)
{
    Repl repl(nb_server);

    repl();
}

bool is_client(int rank, int nb_server)
{
    return rank > nb_server;
}

bool is_server(int rank, int nb_server)
{
    return rank && rank <= nb_server;
}

bool is_repl(int rank)
{
    return !rank;
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

    else if (is_server(rank, nb_server))
        server(rank, nb_server, nb_client);

    else if (is_repl(rank))
        repl(nb_server);

    MPI_Finalize();

    return 0;
}
