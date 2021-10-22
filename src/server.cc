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
    , last_voted_term_(0)
    , next_index_(0)
    , logger_("logs" + std::to_string(rank))
{
    reset_timeout();
}

void Server::send(int message, int rank, int tag)
{
    // FIXME: Should we do something with this request ?
    MPI_Request request = MPI_REQUEST_NULL;

    // Shouldn't message be available until it gets received ?
    // I don't know why this works while message is on the stack and is
    // dealocated right away
    MPI_Isend(&message, 1, MPI_INT, rank, tag, MPI_COMM_WORLD, &request);
}

// Send to all other servers
void Server::send(int message, int tag)
{
    MPI_Request request = MPI_REQUEST_NULL;

    for (auto i = 0; i < size_; i += 2)
        if (i != rank_)
            MPI_Isend(&message, 1, MPI_INT, i, tag, MPI_COMM_WORLD, &request);
}

// receive from a server on a particular tag
int Server::recv(int rank, int tag)
{
    int message;
    MPI_Recv(&message, 1, MPI_INT, rank, tag, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);

    return message;
}
// receive from a server on any tag
int Server::recv(int rank)
{
    int message;
    MPI_Recv(&message, 1, MPI_INT, rank, MPI_ANY_TAG, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);

    return message;
}
// receive from any server on any tag
int Server::recv()
{
    int message;
    MPI_Recv(&message, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);

    return message;
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
    send(0, MessageTag::HEARTBEAT);
    logger_.log(Logger::LogType::INFO, "Sending heatbeat");
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
    status_ = Status::LEADER;
    reset_leader_timeout();
    logger_.log(Logger::LogType::DEBUG, "became the leader");
}

void Server::leader()
{
    // heartbeat back/forth
    // if msg: ask confirmation

    if (now() > heartbeat_timeout_)
        heartbeat();
}

void Server::candidate()
{
    // Next term
    term_++;
    reset_timeout();

    logger_.log(Logger::LogType::INFO, "candidate");

    // Ask for votes for self
    status_ = Status::CANDIDATE;
    send(rank_, MessageTag::REQUEST_VOTE);

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

        recv();

        if (mpi_status.MPI_TAG == MessageTag::VOTE /* && message == rank_*/)
        {
            logger_.log(Logger::LogType::INFO,
                        "got a vote from"
                            + std::to_string(mpi_status.MPI_SOURCE));
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

    message = recv();

    if (mpi_status.MPI_TAG == MessageTag::REQUEST_VOTE
        /* && last_voted_term_ < message */)
    {
        // Vote
        logger_.log(Logger::LogType::INFO,
                    "voting for " + std::to_string(mpi_status.MPI_SOURCE));
        reset_timeout();
        send(mpi_status.MPI_SOURCE, mpi_status.MPI_SOURCE, MessageTag::VOTE);

        last_voted_term_ = message;
    }

    else if (mpi_status.MPI_TAG == MessageTag::APPEND_ENTRIES)
    {
        logger_.log(Logger::LogType::INFO, "AppendEntries");

        reset_timeout();
        // Append entry
    }

    else if (mpi_status.MPI_TAG == MessageTag::HEARTBEAT)
    {
        logger_.log(Logger::LogType::INFO,
                    "received heartbeat from "
                        + std::to_string(mpi_status.MPI_SOURCE));

        reset_timeout();
    }
}
