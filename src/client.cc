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
        Message message;
        message.entry = rank_;
        mpi::send(server, message, MessageTag::CLIENT_REQUEST);

        auto recv_data = mpi::recv(server);

        if (recv_data.tag == MessageTag::REJECT)
        {
            server = recv_data.leader_id;
            std::this_thread::sleep_for(500ms);
        }
        else
        {
            // Request succesfully commited
            break;
        }
    }
}
