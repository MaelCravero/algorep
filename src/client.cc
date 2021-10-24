#include "client.hh"

#include <chrono>
#include <thread>

#include "mpi/mpi.hh"

Client::Client(int rank, int size)
    : rank_(rank)
    , size_(size)
{}

void Client::send_request()
{
    using namespace std::chrono_literals;
    int server = (rank_ + 1) % size_;

    while (true)
    {
        MPI_Send(&rank_, 1, MPI_INT, server, mpi::MessageTag::CLIENT_REQUEST,
                 MPI_COMM_WORLD);

        auto recv_data = mpi::recv(server);

        if (recv_data.tag == mpi::MessageTag::REJECT)
        {
            server = recv_data.message;
            std::this_thread::sleep_for(500ms);
        }
        else
        {
            // Request succesfully commited
            break;
        }
    }
}
