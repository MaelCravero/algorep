#include "server.hh"

#include <chrono>
#include <random>

void server(int rank, int size)
{
    Server server(rank, size);

    while (true)
    {
        server.update();
    }
}

Server::Server(int rank, int size)
    : status_(Status::FOLLOWER)
    , rank_(rank)
    , size_(size)
    , leader_rank_(-1)
    , next_index_(0)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(0.1, 0.5);

    election_timeout_ = dis(gen);
    election_time_remaining_ = election_timeout_;

    previous_time_ = std::chrono::system_clock::now().time_since_epoch();

    term_ = 0;
}

void Server::update()
{
    if (status_ != Status::LEADER)
    {
        // TODO update time

        // auto now = std::chrono::system_clock::now().time_since_epoch();
        // election_time_remaining_ -= fp_ms;

        // TODO: election ?
    }
}
