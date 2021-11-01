#include <iostream>
#include <mpi.h>

#include "client.hh"
#include "repl.hh"
#include "server.hh"

void server(rank rank, int nb_server, int nb_client)
{
    Server server(rank, nb_server, nb_client);

    while (!server.complete())
        server.update();

#ifdef _PERF
    server.write_stats();
#endif
}

void client(rank rank, int nb_server)
{
    auto cmd_file = ".commands_" + std::to_string(rank) + ".txt";
    Client client(rank, nb_server, cmd_file);

    while (!client.done())
    {
        if (client.started())
            client.send_request();
        else
            client.recv_order();
    }
}

void repl(int nb_server, int nb_client)
{
    Repl repl(nb_server, nb_client);

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
        repl(nb_server, nb_client);

    MPI_Finalize();

    return 0;
}
