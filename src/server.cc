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

    /// get a new timeout between `min` and `max` seconds from now
    Server::timestamp get_new_timeout(double min, double max)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dis(min, max);

        int delay = dis(gen) * 1000;
        return now() + std::chrono::milliseconds(delay);
    }

} // namespace

Server::Server(rank rank, int nb_server)
    : status_(Status::FOLLOWER)
    , rank_(rank)
    , nb_server_(nb_server)
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

Server::ServerMessage Server::init_message(int entry)
{
    ServerMessage message;

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

void Server::broadcast(const ServerMessage& message, int tag)
{
    for (auto i = 0; i < nb_server_; i++)
        if (i != rank_)
            mpi::send(i, message, tag);
}

void Server::reset_timeout()
{
    timeout_ = get_new_timeout(0.5, 1);
}

void Server::reset_heartbeat_timeout()
{
    heartbeat_timeout_ = get_new_timeout(0.1, 0.15);
}

void Server::heartbeat()
{
    reset_heartbeat_timeout();
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

int Server::get_log_number() const
{
    return log_entries_.get_commit_index() + 1;
}

void Server::become_leader()
{
    heartbeat();
    status_ = Status::LEADER;
    leader_ = rank_;
    LOG(INFO) << "become the leader";
}

void Server::handle_client_request()
{
    Client::ClientMessage recv_data = mpi::recv<Client::ClientMessage>();

    LOG(INFO) << "received message from client:" << recv_data.source;

    append_entries(term_, recv_data.source, recv_data.entry);
    logs_to_be_commited_[log_entries_.last_log_index().value()] = 1;

    auto message = init_message(recv_data.entry);
    message.client_id = recv_data.source;
    message.log_index = log_entries_.last_log_index().value();

    broadcast(message, MessageTag::APPEND_ENTRIES);
}

void Server::commit_entry(int log_index, int client_id)
{
    log_entries_.commit_next_entry();
    LOG(INFO) << "commited log number: " << log_index;
    logs_to_be_commited_.erase(log_index);

    // Notify client
    auto message = init_message();
    mpi::send(client_id, message, MessageTag::ACKNOWLEDGE);

    LOG(INFO) << "Notify client " << client_id << " for request " << log_index;
}

void Server::handle_ack_append_entry(const ServerMessage& recv_data)
{
    LOG(INFO) << "received acknowledge for log :" << recv_data.log_index
              << " from server " << recv_data.source;

    if (logs_to_be_commited_.contains(recv_data.log_index))
    {
        auto nb_ack =
            ++logs_to_be_commited_[log_entries_.last_log_index().value()];

        if (nb_ack > nb_server_ / 2
            && log_entries_.get_commit_index() == recv_data.log_index - 1)
        {
            commit_entry(recv_data.log_index, recv_data.client_id);

            int i = recv_data.log_index + 1;
            while (logs_to_be_commited_.contains(i)
                   && logs_to_be_commited_[i] > nb_server_ / 2)
            {
                commit_entry(i, log_entries_[i].client_id);
                i++;
            }

            // heartbeat no notify about the new commit index
            heartbeat();
        }
    }
}

void Server::leader()
{
    // heartbeat back/forth
    // if msg: ask confirmation

    if (now() > heartbeat_timeout_)
        heartbeat();

    auto tag = mpi::available_message();
    if (!tag)
        return;

    if (tag.value() == MessageTag::CLIENT_REQUEST)
        return handle_client_request();

    auto recv_data = mpi::recv<ServerMessage>();
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

    // Need a majority of votes
    while (nb_votes <= nb_server_ / 2)
    {
        // on timeout start a new election
        if (now() > timeout_)
            return candidate();

        if (auto tag = mpi::available_message())
        {
            if (tag.value() == MessageTag::VOTE)
            {
                auto recv_data = mpi::recv<ServerMessage>();
                LOG(INFO) << "got a vote from " << recv_data.source;
                nb_votes++;
            }

            else if (tag.value() == MessageTag::HEARTBEAT)
            {
                auto recv_data = mpi::recv<ServerMessage>();

                if (recv_data.term >= term_)
                {
                    LOG(INFO) << "received heartbeat from more legitimate "
                                 "leader or server that won the election: "
                              << recv_data.source;

                    leader_ = recv_data.source;
                    status_ = Status::FOLLOWER;

                    update_term(recv_data.term);

                    auto message = init_message();
                    mpi::send(recv_data.source, message,
                              MessageTag::ACKNOWLEDGE);

                    reset_timeout();

                    return;
                }
            }

            else if (tag.value() == MessageTag::CLIENT_REQUEST)
            {
                auto recv_data = mpi::recv<Client::ClientMessage>();
                auto message = init_message();
                mpi::send(recv_data.source, message, MessageTag::REJECT);
            }

            else
            {
                auto recv_data = mpi::recv<Client::ClientMessage>();
                LOG(DEBUG) << "received message from :" << recv_data.source
                           << " with tag " << recv_data.tag;
            }
        }
    }

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

    if (auto tag = mpi::available_message())
    {
        if (tag.value() == MessageTag::CLIENT_REQUEST)
        {
            auto recv_data = mpi::recv<Client::ClientMessage>();
            LOG(INFO) << "received message from client:" << recv_data.source;

            auto message = init_message();
            mpi::send(recv_data.source, message, MessageTag::REJECT);
            return;
        }

        auto recv_data = mpi::recv<ServerMessage>();

        if (recv_data.tag == MessageTag::REQUEST_VOTE && term_ < recv_data.term)
        {
            vote(recv_data.source);

            update_term(recv_data.term);
        }

        else if (recv_data.tag == MessageTag::APPEND_ENTRIES)
        {
            leader_ = recv_data.source;
            reset_timeout();

            append_entries(recv_data.term, recv_data.client_id,
                           recv_data.entry);

            mpi::send(leader_, recv_data,
                      MessageTag::ACKNOWLEDGE_APPEND_ENTRIES);
        }

        else if (recv_data.tag == MessageTag::HEARTBEAT)
        {
            LOG(INFO) << "received heartbeat from " << recv_data.source;

            leader_ = recv_data.source;

            while (recv_data.leader_commit > log_entries_.get_commit_index())
            {
                log_entries_.commit_next_entry();
                LOG(INFO) << "commited log number: " << get_log_number();
            }

            if (term_ < recv_data.term)
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
}
