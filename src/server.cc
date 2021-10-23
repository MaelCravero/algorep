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
    , leader_(-1)
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
    broadcast(0, MessageTag::HEARTBEAT);
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
    LOG(INFO) << "became the leader";
}

void Server::leader()
{
    // heartbeat back/forth
    // if msg: ask confirmation

    if (now() > heartbeat_timeout_)
        heartbeat();
}

void Server::update_term()
{
    update_term(term_ + 1);
}

void Server::update_term(int term)
{
    term_ = term;

    LOG(INFO) << "actual term: " << term_;
}

void Server::candidate()
{
    // Next term
    reset_timeout();

    update_term();

    LOG(INFO) << "candidate";

    status_ = Status::CANDIDATE;

    // Ask for votes
    broadcast(term_, MessageTag::REQUEST_VOTE);

    // Receive answers
    MPI_Status mpi_status;

    int nb_votes = 1; // Vote for himself

    // Need half of server which is a quarter of processes
    while (nb_votes <= size_ / 4)
    {
        if (now() > timeout_)
            return candidate();

        int flag;
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag,
                   &mpi_status);
        if (!flag)
        {
            continue;
        }

        mpi::recv();

        if (mpi_status.MPI_TAG == MessageTag::VOTE /* && message == rank_*/)
        {
            LOG(INFO) << "got a vote from " << mpi_status.MPI_SOURCE;

            nb_votes++;
        }

        if (mpi_status.MPI_TAG == MessageTag::HEARTBEAT)
        {
            reset_timeout();
            return;
        }
    }

    if (nb_votes > size_ / 4)
        become_leader();
}

void Server::follower()
{
    status_ = Status::FOLLOWER;

    MPI_Status mpi_status;
    int message;

    int flag;
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &mpi_status);

    if (!flag)
    {
        return;
    }

    message = mpi::recv();

    if (mpi_status.MPI_TAG == MessageTag::REQUEST_VOTE && term_ < message)
    {
        // Vote
        LOG(INFO) << "voting for " << mpi_status.MPI_SOURCE;
        reset_timeout();
        mpi::send(mpi_status.MPI_SOURCE, mpi_status.MPI_SOURCE,
                  MessageTag::VOTE);

        update_term(message);
    }

    else if (mpi_status.MPI_TAG == MessageTag::APPEND_ENTRIES)
    {
        LOG(INFO) << "AppendEntries";
        leader_ = mpi_status.MPI_SOURCE;

        reset_timeout();
        // Append entry
    }

    else if (mpi_status.MPI_TAG == MessageTag::HEARTBEAT)
    {
        LOG(INFO) << "received heartbeat from " << mpi_status.MPI_SOURCE;

        leader_ = mpi_status.MPI_SOURCE;

        reset_timeout();
    }
}
