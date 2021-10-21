#include "server.hh"

#include <chrono>
#include <random>

namespace
{
    Server::timestamp now()
    {
        return std::chrono::system_clock::now().time_since_epoch();
    }

} // namespace

Server::Server(rank rank, int size)
    : status_(Status::FOLLOWER)
    , rank_(rank)
    , size_(size)
    , leader_(-1)
    , timeout_()
    , term_(0)
    , next_index_(0)
{}

void Server::reset_timeout()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(0.1, 0.5);

    int delay = dis(gen) * 1000;
    timeout_ = now() + std::chrono::milliseconds(delay);
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

void Server::leader()
{
    // heartbeat back/forth
    // if msg: ask confirmation
}

void Server::candidate()
{
    // ask votes
    // if majority declare leader
}

void Server::follower()
{
    // vote
    // ack msg
    // if heartbeat, reset timeout

    // We will most likely only be sending ids, we can use the tag field of send
    // for determining the type of the message
}
