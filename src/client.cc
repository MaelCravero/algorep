#include "client.hh"

#include <assert.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "mpi/mpi.hh"
#include "repl.hh"
#include "rpc/rpc.hh"
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
    , done_(false)
    , command_list_(init_commands(cmd_file))
    , mpi_()
{
#ifdef _DEBUG
    std::cout << "client " << rank_ << "has PID " << getpid() << std::endl;
#endif
}

bool Client::done() const
{
    return done_;
}

void Client::send_request()
{
    if (done_)
        return;

    rpc::ClientRequest message{rank_, request_id_, command_list_[request_id_]};

    utils::Timeout timeout(2, 2.6);

    mpi_.send(server_, message, MessageTag::CLIENT_REQUEST);

    int count_retry = 0;

    while (!done_ && count_retry < nb_server_ * 3)
    {
        if (timeout)
        {
            server_ = server_ % nb_server_ + 1;

            mpi_.send(server_, message, MessageTag::CLIENT_REQUEST);
            timeout.reset();

            count_retry++;
        }

        if (mpi_.available_message(server_))
        {
            auto recv_data = mpi_.recv<rpc::ClientRequestResponse>(server_);

            if (!recv_data.value)
            {
                server_ = recv_data.leader;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                mpi_.send(server_, message, MessageTag::CLIENT_REQUEST);
                timeout.reset();

                count_retry++;
            }

            else
            {
                request_id_++;
                if (request_id_ >= command_list_.size())
                    done_ = true;

                return;
            }
        }
        recv_order();
    }
}

bool Client::started()
{
    return started_;
}

bool Client::recv_order()
{
    auto tag = mpi_.available_message(MPI_ANY_SOURCE, MessageTag::REPL);

    if (!tag)
        return false;

    auto recv_data = mpi_.recv<rpc::Repl>();

    if (recv_data.order == Repl::Order::STOP)
        done_ = true;
    else if (recv_data.order == Repl::Order::BEGIN)
        started_ = true;
    return true;
}
