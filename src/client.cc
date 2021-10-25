#include "client.hh"

#include <chrono>
#include <thread>

#include "mpi/mpi.hh"

Client::Client(int rank, int nb_server)
    : rank_(rank)
    , nb_server_(nb_server)
    , server_(rank % nb_server)
{}

bool Client::send_request()
{
    using namespace std::chrono_literals;

    Message message;
    message.entry = rank_;
    mpi::send(server_, message, MessageTag::CLIENT_REQUEST);

    auto recv_data = mpi::recv<Message>(server_);

    if (recv_data.tag == MessageTag::REJECT)
    {
        server_ = recv_data.leader_id;
        std::this_thread::sleep_for(500ms);
        return false;
    }

    else
    {
        return true;
    }
}
