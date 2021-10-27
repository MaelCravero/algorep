#include "client.hh"

#include <assert.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "mpi/mpi.hh"
#include "repl.hh"
#include "server.hh"

namespace
{
    Client::command_list init_commands(std::string cmd_file)
    {
        Client::command_list res;

        auto input = std::ifstream(cmd_file);

        std::string line;
        while (input >> line)
            res.push_back(line);

        return res;
    }
} // namespace

Client::Client(int rank, int nb_server, std::string cmd_file)
    : rank_(rank)
    , nb_server_(nb_server)
    , server_(rank % nb_server + 1)
    , request_id_(0)
    , started_(false)
    , command_list_(init_commands(cmd_file))
{
#if 0
    std::cout << "client " << rank_ << "has PID " << getpid() << std::endl;
#endif
}

bool Client::done() const
{
    return request_id_ >= command_list_.size();
}

bool Client::send_request()
{
    ClientMessage message;

    message.entry = rank_;
    std::memcpy(message.command, command_list_[request_id_].data(), 64);
    message.request_id = request_id_++;

    utils::timestamp timeout = utils::get_new_timeout(2, 2.6);

    mpi::send(server_, message, MessageTag::CLIENT_REQUEST);

    int count_retry = 0;

    while (count_retry < nb_server_ * 3)
    {
        if (utils::now() > timeout)
        {
            server_ = server_ % nb_server_ + 1;

            mpi::send(server_, message, MessageTag::CLIENT_REQUEST);
            timeout = utils::get_new_timeout(2, 2.6);

            count_retry++;
        }

        if (mpi::available_message(server_))
        {
            auto recv_data = mpi::recv<Server::ServerMessage>(server_);

            if (recv_data.tag == MessageTag::REJECT)
            {
                server_ = recv_data.leader_id;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                mpi::send(server_, message, MessageTag::CLIENT_REQUEST);
                timeout = utils::get_new_timeout(2, 2.6);

                count_retry++;
            }

            else
            {
                return true;
            }
        }
    }

    return false;
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
