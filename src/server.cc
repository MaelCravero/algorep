#include "server.hh"

#include <chrono>
#include <mpi/mpi.hh>
#include <random>

#define LOG(mode) logger_ << utils::Logger::LogType::mode

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
    , leader_(rank)
    , timeout_()
    , term_(0)
    , next_index_(0)
    , logger_("logs" + std::to_string(rank))
{
    reset_timeout();
}

// Send to all other servers
void Server::broadcast(int message, int tag)
{
    // FIXME This could be done better

    MPI_Request request = MPI_REQUEST_NULL;

    for (auto i = 0; i < size_; i += 2)
        if (i != rank_)
            MPI_Isend(&message, 1, MPI_INT, i, tag, MPI_COMM_WORLD, &request);
}

void Server::reset_timeout()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(5, 10);

    int delay = dis(gen) * 1000;
    timeout_ = now() + std::chrono::milliseconds(delay);
}

void Server::reset_leader_timeout()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(1, 2);

    int delay = dis(gen) * 1000;
    heartbeat_timeout_ = now() + std::chrono::milliseconds(delay);
}

void Server::heartbeat()
{
    reset_leader_timeout();
    broadcast(0, mpi::MessageTag::HEARTBEAT);
    LOG(INFO) << "sending heatbeat";
}

void Server::update()
{
    if (status_ != Status::LEADER)
    {
        if (now() > timeout_)
            return candidate();

        follower();
    }

    else
        leader();
}

void Server::become_leader()
{
    reset_leader_timeout();
    status_ = Status::LEADER;
    leader_ = rank_;
    LOG(INFO) << "become the leader";
}

void Server::leader()
{
    // heartbeat back/forth
    // if msg: ask confirmation

    if (now() > heartbeat_timeout_)
        heartbeat();

    if (!mpi::available_message())
        return;

    auto recv_data = mpi::recv();
    if (recv_data.tag == mpi::MessageTag::CLIENT_REQUEST)
    {
        LOG(INFO) << "received message from client:" << recv_data.source;
        // TODO replicate log
        mpi::send(recv_data.source, leader_, mpi::MessageTag::ACKNOWLEDGE);
    }

    else
    {
        LOG(DEBUG) << "received message from :" << recv_data.source
                   << " message : " << recv_data.message << " with tag "
                   << recv_data.tag;
    }
}

void Server::update_term()
{
    update_term(term_ + 1);
}

void Server::update_term(int term)
{
    term_ = term;

    LOG(INFO) << "current term: " << term_;
}

void Server::candidate()
{
    // Next term
    reset_timeout();

    update_term();

    LOG(INFO) << "candidate";

    status_ = Status::CANDIDATE;

    // Ask for votes
    broadcast(term_, mpi::MessageTag::REQUEST_VOTE);

    int nb_votes = 1; // Vote for himself

    // Need half of server which is a quarter of processes
    while (nb_votes <= size_ / 4)
    {
        if (now() > timeout_)
            return candidate();

        if (!mpi::available_message())
            continue;

        auto recv_data = mpi::recv();

        if (recv_data.tag == mpi::MessageTag::VOTE)
        {
            LOG(INFO) << "got a vote from " << recv_data.source;

            nb_votes++;
        }

        else if (recv_data.tag == mpi::MessageTag::HEARTBEAT)
        {
            reset_timeout();
            return;
        }

        else
        {
            LOG(DEBUG) << "received message from :" << recv_data.source
                       << " message : " << recv_data.message << " with tag "
                       << recv_data.tag;
        }
    }

    if (nb_votes > size_ / 4)
        become_leader();
}

void Server::vote(int server, int message)
{
    LOG(INFO) << "voting for " << server;
    reset_timeout();
    mpi::send(server, term_, mpi::MessageTag::VOTE);

    update_term(message);
}

void Server::append_entries(int server, int message)
{
    LOG(INFO) << "AppendEntries";
    leader_ = server;

    reset_timeout();
    // Append entry
}

void Server::follower()
{
    status_ = Status::FOLLOWER;

    if (!mpi::available_message())
        return;

    auto recv_data = mpi::recv();

    if (recv_data.tag == mpi::MessageTag::REQUEST_VOTE
        && term_ < recv_data.message)
        vote(recv_data.source, recv_data.message);

    else if (recv_data.tag == mpi::MessageTag::APPEND_ENTRIES)
        append_entries(recv_data.source, recv_data.message);

    else if (recv_data.tag == mpi::MessageTag::HEARTBEAT)
    {
        LOG(INFO) << "received heartbeat from " << recv_data.source;

        leader_ = recv_data.source;

        reset_timeout();
    }

    else if (recv_data.tag == mpi::MessageTag::CLIENT_REQUEST)
    {
        LOG(INFO) << "received message from client:" << recv_data.source;
        mpi::send(recv_data.source, leader_, mpi::MessageTag::REJECT);
    }

    else
    {
        LOG(DEBUG) << "received message from :" << recv_data.source
                   << " message : " << recv_data.message << " with tag "
                   << recv_data.tag;
    }
}
