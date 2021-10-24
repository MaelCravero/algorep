#include "server.hh"

#include <chrono>
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
    , match_index_(0)
    , log_entries_("entries_server" + std::to_string(rank) + ".log")
    , logs_to_be_commited_()
    , logger_("log_server" + std::to_string(rank) + ".log")
{
    reset_timeout();
}

Message Server::init_message(int entry)
{
    Message message;

    message.term = term_;
    auto log_index = log_entries_.last_log_index();
    message.last_log_index = log_index ? log_index.value() : -1;

    auto log_term = log_entries_.last_log_term();
    message.last_log_term = log_term ? log_term.value() : -1;

    message.entry = entry;

    message.leader_id = leader_;
    message.leader_commit = log_entries_.get_commit_index();

    return message;
}

// Send to all other servers
void Server::broadcast(const Message& message, int tag)
{
    // FIXME This could be done better

    for (auto i = 0; i < size_; i += 2)
        if (i != rank_)
            mpi::send(i, message, tag);
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
    auto message = init_message();
    broadcast(message, MessageTag::HEARTBEAT);
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

void Server::handle_client_request(const Message& recv_data)
{
    LOG(INFO) << "received message from client:" << recv_data.source;

    append_entries(term_, recv_data.source, recv_data.entry);
    logs_to_be_commited_[log_entries_.last_log_index().value()] = 1;

    auto message = init_message(recv_data.entry);
    message.client_id = recv_data.source;
    message.log_index = log_entries_.last_log_index().value();

    broadcast(message, MessageTag::APPEND_ENTRIES);
}

void Server::handle_ack_append_entry(const Message& recv_data)
{
    LOG(INFO) << "received acknowledge for log :" << recv_data.log_index
              << " from server " << recv_data.source;

    if (logs_to_be_commited_.contains(recv_data.log_index))
    {
        auto nb_ack =
            logs_to_be_commited_[log_entries_.last_log_index().value()] + 1;
        logs_to_be_commited_[log_entries_.last_log_index().value()] = nb_ack;

        // FIXME: div 4
        if (nb_ack > size_ / 4
            && log_entries_.get_commit_index() == recv_data.log_index - 1)
        {
            log_entries_.commit_next_entry();
            logs_to_be_commited_.erase(recv_data.log_index);

            // Notify client
            auto message = init_message();
            mpi::send(recv_data.client_id, message, MessageTag::ACKNOWLEDGE);

            LOG(INFO) << "Notify client " << recv_data.client_id
                      << " for request " << recv_data.log_index;

            int i = recv_data.log_index + 1;
            while (logs_to_be_commited_.contains(i)
                   && logs_to_be_commited_[i] > size_ / 4)
            {
                log_entries_.commit_next_entry();
                logs_to_be_commited_.erase(recv_data.log_index);

                // Notify client
                auto message = init_message();
                mpi::send(log_entries_[i].client_id, message,
                          MessageTag::ACKNOWLEDGE);

                LOG(INFO) << "Notify client " << recv_data.client_id
                          << " for request " << recv_data.log_index;

                i++;
            }
        }
    }
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
    if (recv_data.tag == MessageTag::CLIENT_REQUEST)
        handle_client_request(recv_data);

    if (recv_data.tag == MessageTag::ACKNOWLEDGE_APPEND_ENTRIES)
        handle_ack_append_entry(recv_data);

    else if (recv_data.tag == MessageTag::HEARTBEAT && recv_data.term > term_)
    {
        LOG(INFO) << "received heartbeat from more legitimate leader: "
                  << recv_data.source;

        leader_ = recv_data.source;
        status_ = Status::FOLLOWER;

        update_term(recv_data.term);

        auto message = init_message();
        mpi::send(recv_data.source, message, MessageTag::ACKNOWLEDGE);

        reset_timeout();
    }

    else
    {
        LOG(DEBUG) << "received message from :" << recv_data.source
                   << " with tag " << recv_data.tag;
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
    reset_timeout();

    update_term();

    LOG(INFO) << "candidate";

    status_ = Status::CANDIDATE;

    // Ask for votes
    auto message = init_message();
    broadcast(message, MessageTag::REQUEST_VOTE);

    int nb_votes = 1; // Vote for himself

    // Need half of server which is a quarter of processes
    while (nb_votes <= size_ / 4)
    {
        if (now() > timeout_)
            return candidate();

        if (!mpi::available_message())
            continue;

        auto recv_data = mpi::recv();

        if (recv_data.tag == MessageTag::VOTE)
        {
            LOG(INFO) << "got a vote from " << recv_data.source;

            nb_votes++;
        }

        else if (recv_data.tag == MessageTag::HEARTBEAT)
        {
            reset_timeout();
            return;
        }

        else
        {
            LOG(DEBUG) << "received message from :" << recv_data.source
                       << " with tag " << recv_data.tag;
        }
    }

    if (nb_votes > size_ / 4)
        become_leader();
}

void Server::vote(int server)
{
    LOG(INFO) << "voting for " << server;
    reset_timeout();

    auto message = init_message();
    mpi::send(server, message, MessageTag::VOTE);
}

void Server::append_entries(int term, int client_id, int data)
{
    LOG(INFO) << "AppendEntries";

    log_entries_.append_entry(term, client_id, data);
}

void Server::follower()
{
    status_ = Status::FOLLOWER;

    if (!mpi::available_message())
        return;

    auto recv_data = mpi::recv();

    if (recv_data.tag == MessageTag::REQUEST_VOTE && term_ < recv_data.term)
    {
        vote(recv_data.source);

        update_term(recv_data.term);
    }

    else if (recv_data.tag == MessageTag::APPEND_ENTRIES)
    {
        leader_ = recv_data.source;
        reset_timeout();

        append_entries(recv_data.term, recv_data.client_id, recv_data.entry);

        mpi::send(leader_, recv_data, MessageTag::ACKNOWLEDGE_APPEND_ENTRIES);
    }

    else if (recv_data.tag == MessageTag::HEARTBEAT)
    {
        LOG(INFO) << "received heartbeat from " << recv_data.source;

        leader_ = recv_data.source;

        while (recv_data.leader_commit > log_entries_.get_commit_index())
        {
            log_entries_.commit_next_entry();
        }

        reset_timeout();
    }

    else if (recv_data.tag == MessageTag::CLIENT_REQUEST)
    {
        LOG(INFO) << "received message from client:" << recv_data.source;

        auto message = init_message();
        mpi::send(recv_data.source, message, MessageTag::REJECT);
    }

    else
    {
        LOG(DEBUG) << "received message from :" << recv_data.source
                   << " with tag " << recv_data.tag;
    }
}
