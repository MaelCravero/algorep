#include "client.hh"

#include <chrono>
#include <thread>

#include "mpi/mpi.hh"
#include "repl.hh"
#include "server.hh"

Client::Client(int rank, int nb_server)
    : rank_(rank)
    , nb_server_(nb_server)
    , server_(rank % nb_server + 1)
    , started_(false)
{}

bool Client::send_request()
{
    using namespace std::chrono_literals;

    ClientMessage message;
    message.entry = rank_;
    mpi::send(server_, message, MessageTag::CLIENT_REQUEST);

    auto recv_data = mpi::recv<Server::ServerMessage>(server_);

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

bool Client::started()
{
    return started_;
}

bool Client::recv_order()
{
    auto tag = mpi::available_message(MPI_ANY_SOURCE, MessageTag::REPL);

    if (!tag)
        return false;

    mpi::recv<Repl::ReplMessage>();

    started_ = true;
    return true;
}
