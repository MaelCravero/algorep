#include "server.hh"

#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "repl.hh"

#define LOG(mode) logger_ << utils::Logger::LogType::mode

Server::Server(rank rank, int nb_server)
    : status_(Status::FOLLOWER)
    , rank_(rank)
    , nb_server_(nb_server)
    , leader_(rank)
    , speed_mod_(1)
    , timeout_()
    , term_(0)
    , has_crashed_(false)
    , next_index_(nb_server + 1)
    , match_index_(nb_server + 1)
    , log_entries_("entries_server" + std::to_string(rank) + ".log")
    , logs_to_be_commited_()
    , logger_("log_server" + std::to_string(rank) + ".log")
{
    reset_timeout();
    LOG(DEBUG) << "server " << rank_ << "has PID " << getpid();
}

void Server::init_next_index()
{
    int last_index = log_entries_.last_log_index();
    for (int i = 1; i <= nb_server_; i++)
        next_index_[i] = last_index + 1;
}

Server::ServerMessage Server::init_message(int entry)
{
    ServerMessage message;

    message.term = term_;
    message.last_log_index = log_entries_.last_log_index();

    message.last_log_term = log_entries_.last_log_term();

    message.entry = entry;

    message.leader_id = leader_;
    message.leader_commit = log_entries_.get_commit_index();

    return message;
}

void Server::broadcast(const ServerMessage& message, int tag)
{
    for (auto i = 1; i <= nb_server_; i++)
        if (i != rank_)
            mpi::send(i, message, tag);
}

void Server::reset_timeout()
{
    timeout_ = utils::get_new_timeout(0.5 * speed_mod_, 1 * speed_mod_);
}

void Server::reset_heartbeat_timeout()
{
    heartbeat_timeout_ =
        utils::get_new_timeout(0.1 * speed_mod_, 0.15 * speed_mod_);
}

void Server::update()
{
    if (speed_mod_ > 1)
        std::this_thread::sleep_for(
            std::chrono::milliseconds(20 * (speed_mod_ / 3)));

    if (status_ != Status::LEADER)
    {
        if (utils::now() > timeout_ && !has_crashed_)
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
    init_next_index();
    heartbeat();
    status_ = Status::LEADER;
    leader_ = rank_;
    LOG(INFO) << "become the leader";
}

void Server::heartbeat()
{
    reset_heartbeat_timeout();
    auto message = init_message();

    for (int i = 1; i <= nb_server_; i++)
    {
        if (i == rank_)
            continue;

        if (next_index_[i] > log_entries_.last_log_index())
        {
            LOG(INFO) << "server: " << i << " is up to date, next_index is "
                      << next_index_[i];

            mpi::send(i, message, MessageTag::HEARTBEAT);
        }

        else
        {
            auto next_entry = log_entries_[next_index_[i]];
            message.client_id = next_entry.client_id;
            message.request_id = next_entry.request_id;
            message.entry = next_entry.data;
            message.term = term_;

            message.last_log_index = next_index_[i] - 1;
            message.last_log_term = next_index_[i] - 1 < 0
                ? -1
                : log_entries_[next_index_[i] - 1].term;

            LOG(INFO) << "Sending append entries to " << i
                      << ", client_id: " << message.client_id
                      << ", request_id: " << message.request_id
                      << ", entry: " << message.entry << ", term: " << term_
                      << ", last_log_index: " << message.last_log_index
                      << ", last_log_term: " << message.last_log_term;

            mpi::send(i, message, MessageTag::APPEND_ENTRIES);
        }
    }
}

void Server::handle_client_request(int src, int tag)
{
    LOG(DEBUG) << "recv from client at " << __FILE__ << ":" << __LINE__;
    Client::ClientMessage recv_data =
        mpi::recv<Client::ClientMessage>(src, tag);

    LOG(INFO) << "received message from client:" << recv_data.source;

    append_entries(term_, recv_data.source, recv_data.entry,
                   recv_data.request_id);

    logs_to_be_commited_[log_entries_.last_log_index()] = 1;

    heartbeat();
}

void Server::handle_repl_request(int src)
{
    LOG(DEBUG) << "recv from repl at " << __FILE__ << ":" << __LINE__;
    auto message = mpi::recv<Repl::ReplMessage>(src, MessageTag::REPL);

    if (message.order == Repl::Order::PRINT)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(30 * rank_));

        std::cout << "Server: " << rank_ << "/" << nb_server_ << "\n";
        std::cout << "   PID: " << getpid() << "\n";
        std::cout << " Speed: " << speed_mod_ << "\n";
        std::cout << " Crash: " << std::boolalpha << has_crashed_ << "\n";
        std::cout << "Leader: " << leader_ << "\n";
        if (leader_ == rank_)
        {
            std::cout << "NxtIdx: ";
            for (int i = 1; i <= nb_server_; i++)
            {
                std::cout << next_index_[i] << " ";
            }
            std::cout << "\n";
        }
        std::cout << "  Term: " << term_ << "\n";
        std::cout << "NbLogs: " << log_entries_.get_commit_index() + 1 << "/"
                  << log_entries_.size() << "\n\n";
    }
    if (message.order == Repl::Order::SPEED)
        speed_mod_ = message.speed_level;
    if (message.order == Repl::Order::CRASH)
        has_crashed_ = true;
    if (message.order == Repl::Order::RECOVERY)
    {
        LOG(DEBUG) << "recover";
        has_crashed_ = false;
        status_ = Status::FOLLOWER;
        reset_timeout();
    }
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

void Server::handle_accept_append_entry(const ServerMessage& recv_data)
{
    LOG(INFO) << "received acknowledge for log :" << recv_data.log_index
              << " from server " << recv_data.source;

    next_index_[recv_data.source] = recv_data.log_index + 1;
    match_index_[recv_data.source]++;

    LOG(INFO) << "server :" << recv_data.source
              << " index: " << next_index_[recv_data.source];

    if (logs_to_be_commited_.contains(recv_data.log_index))
    {
        auto nb_ack = ++logs_to_be_commited_[recv_data.log_index];

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
        }
    }
}

void Server::handle_reject_append_entry(const ServerMessage& recv_data)
{
    LOG(INFO) << "received reject for log :" << recv_data.log_index
              << " from server " << recv_data.source;

    next_index_[recv_data.source]--;
    if (next_index_[recv_data.source] < 0)
        next_index_[recv_data.source] = 0;
}

void Server::ignore_messages()
{
    int flag;
    mpi::status status;
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
    while (flag)
    {
        if (status.MPI_TAG == MessageTag::REPL)
            break;

        if (status.MPI_TAG == MessageTag::CLIENT_REQUEST)
        {
            LOG(DEBUG) << "recv from client at " << __FILE__ << ":" << __LINE__;
            mpi::recv<Client::ClientMessage>(status.MPI_SOURCE, status.MPI_TAG);
        }
        else
        {
            LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
            mpi::recv<ServerMessage>(status.MPI_SOURCE, status.MPI_TAG);
        }
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
    }
}

void Server::leader()
{
    // heartbeat back/forth
    // if msg: ask confirmation

    if (utils::now() > heartbeat_timeout_ && !has_crashed_)
        heartbeat();

    if (has_crashed_)
        ignore_messages();

    int flag;
    mpi::status status;
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);

    if (!flag)
        return;

    if (status.MPI_TAG == MessageTag::REPL)
        return handle_repl_request(status.MPI_SOURCE);

    if (has_crashed_)
        return;

    if (status.MPI_TAG == MessageTag::CLIENT_REQUEST)
        return handle_client_request(status.MPI_SOURCE, status.MPI_TAG);

    LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
    auto recv_data =
        mpi::recv<ServerMessage>(status.MPI_SOURCE, status.MPI_TAG);

    if (recv_data.tag == MessageTag::ACCEPT_APPEND_ENTRIES)
        handle_accept_append_entry(recv_data);

    if (recv_data.tag == MessageTag::REJECT_APPEND_ENTRIES)
        handle_reject_append_entry(recv_data);

    else if ((recv_data.tag == MessageTag::APPEND_ENTRIES
              || recv_data.tag == MessageTag::HEARTBEAT)
             && recv_data.term > term_)
    {
        LOG(INFO) << "received message from more legitimate leader: "
                  << recv_data.source;

        leader_ = recv_data.source;
        status_ = Status::FOLLOWER;

        update_term(recv_data.term);

        // auto message = init_message();
        // mpi::send(recv_data.source, message, MessageTag::ACKNOWLEDGE);

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
        if (utils::now() > timeout_)
            return candidate();

        int flag;
        mpi::status status;
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);

        if (flag)
        {
            if (status.MPI_TAG == MessageTag::REPL)
                return handle_repl_request(status.MPI_SOURCE);

            if (has_crashed_)
                return;

            if (status.MPI_TAG == MessageTag::VOTE)
            {
                LOG(DEBUG) << "recv from server at " << __FILE__ << ":"
                           << __LINE__;
                auto recv_data =
                    mpi::recv<ServerMessage>(status.MPI_SOURCE, status.MPI_TAG);
                LOG(INFO) << "got a vote from " << recv_data.source;
                nb_votes++;
            }

            else if (status.MPI_TAG == MessageTag::HEARTBEAT)
            {
                LOG(DEBUG) << "recv from server at " << __FILE__ << ":"
                           << __LINE__;
                auto recv_data =
                    mpi::recv<ServerMessage>(status.MPI_SOURCE, status.MPI_TAG);

                if (recv_data.term >= term_)
                {
                    LOG(INFO) << "received heartbeat from more legitimate "
                                 "leader or server that won the election: "
                              << recv_data.source;

                    leader_ = recv_data.source;
                    status_ = Status::FOLLOWER;

                    update_term(recv_data.term);

                    // auto message = init_message();
                    // mpi::send(recv_data.source, message,
                    //          MessageTag::ACKNOWLEDGE);

                    reset_timeout();

                    return;
                }
            }

            else if (status.MPI_TAG == MessageTag::CLIENT_REQUEST)
            {
                LOG(DEBUG) << "recv from client at " << __FILE__ << ":"
                           << __LINE__;
                auto recv_data = mpi::recv<Client::ClientMessage>(
                    status.MPI_SOURCE, status.MPI_TAG);
                auto message = init_message();
                mpi::send(recv_data.source, message, MessageTag::REJECT);
            }

            else
            {
                LOG(DEBUG) << "recv from server at " << __FILE__ << ":"
                           << __LINE__;
                auto recv_data =
                    mpi::recv<ServerMessage>(status.MPI_SOURCE, status.MPI_TAG);
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

void Server::append_entries(int term, int client_id, int data, int request_id)
{
    LOG(INFO) << "AppendEntries: index = " << log_entries_.size()
              << ", term: " << term << ", client: " << client_id
              << ", entry: " << data << ", request_id " << request_id;

    log_entries_.append_entry(term, client_id, data, request_id);
}

void Server::follower()
{
    status_ = Status::FOLLOWER;

    if (has_crashed_)
        ignore_messages();

    int flag;
    mpi::status status;
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);

    if (!flag)
        return;

    LOG(DEBUG) << "available tag is " << status.MPI_TAG;
    if (status.MPI_TAG == MessageTag::REPL)
        return handle_repl_request(status.MPI_SOURCE);

    if (has_crashed_)
        return;

    if (status.MPI_TAG == MessageTag::CLIENT_REQUEST)
    {
        LOG(DEBUG) << "recv from client at " << __FILE__ << ":" << __LINE__;
        auto recv_data =
            mpi::recv<Client::ClientMessage>(status.MPI_SOURCE, status.MPI_TAG);
        LOG(INFO) << "received message from client:" << recv_data.source;

        auto message = init_message();
        mpi::send(recv_data.source, message, MessageTag::REJECT);
        return;
    }

    LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
    auto recv_data =
        mpi::recv<ServerMessage>(status.MPI_SOURCE, status.MPI_TAG);

    if (recv_data.tag == MessageTag::REQUEST_VOTE)
    {
        if (term_ < recv_data.term)
        {
            update_term(recv_data.term);

            // TODO: add bool has_voted_
            if (log_entries_.last_log_index() <= recv_data.last_log_index
                && log_entries_.last_log_term() <= recv_data.last_log_term)
            {
                vote(recv_data.source);
            }
        }
    }

    else if (recv_data.tag == MessageTag::APPEND_ENTRIES)
    {
        leader_ = recv_data.source;
        reset_timeout();

        if (recv_data.term < term_)
        {
            mpi::send(leader_, recv_data, MessageTag::REJECT_APPEND_ENTRIES);
            return;
        }
        if (recv_data.last_log_index > log_entries_.last_log_index()
            || recv_data.last_log_term > log_entries_.last_log_term())
        {
            LOG(INFO) << "rejecting append entries term:" << recv_data.term
                      << "|" << term_
                      << " last log_index : " << recv_data.last_log_index << "|"
                      << log_entries_.last_log_index()
                      << " log term:" << recv_data.last_log_term << "|"
                      << log_entries_.last_log_term();

            mpi::send(leader_, recv_data, MessageTag::REJECT_APPEND_ENTRIES);
            return;
        }

        // last_log_index == prev_log_index
        if (log_entries_.last_log_index() > recv_data.last_log_index
            && recv_data.term
                != log_entries_[recv_data.last_log_index + 1].term)
        {
            log_entries_.delete_from_index(recv_data.last_log_index + 1);
        }

        append_entries(recv_data.term, recv_data.client_id, recv_data.entry,
                       recv_data.request_id);

        auto message = init_message();
        message.log_index = recv_data.last_log_index + 1;
        message.client_id = recv_data.client_id;

        while (recv_data.leader_commit > log_entries_.get_commit_index())
        {
            if (!log_entries_.commit_next_entry())
                break;
            LOG(INFO) << "commited log number: " << get_log_number();
        }

        if (term_ < recv_data.term)
            update_term(recv_data.term);

        mpi::send(leader_, message, MessageTag::ACCEPT_APPEND_ENTRIES);
    }

    else if (recv_data.tag == MessageTag::HEARTBEAT)
    {
        LOG(INFO) << "received heartbeat from " << recv_data.source;

        leader_ = recv_data.source;

        while (recv_data.leader_commit > log_entries_.get_commit_index())
        {
            if (!log_entries_.commit_next_entry())
                break;
            LOG(INFO) << "commited log number: " << get_log_number();
        }

        if (term_ < recv_data.term)
            update_term(recv_data.term);

        // auto message = init_message();
        if (recv_data.last_log_index > log_entries_.last_log_index())
            mpi::send(recv_data.source, recv_data,
                      MessageTag::REJECT_APPEND_ENTRIES);

        reset_timeout();
    }

    else
    {
        LOG(DEBUG) << "received message from :" << recv_data.source
                   << " with tag " << recv_data.tag;
    }
}
