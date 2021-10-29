#include "server.hh"

#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "repl.hh"

#define LOG(mode) logger_ << utils::Logger::LogType::mode

//------------------------------------------------------------------//
//                           Constructor                            //
//------------------------------------------------------------------//

Server::Server(rank rank, int nb_server, int nb_request)
    : status_(Status::FOLLOWER)
    , rank_(rank)
    , nb_server_(nb_server)
    , nb_request_(nb_request)
    , leader_(rank)
    , speed_mod_(1)
    , timeout_(0.5, 1)
    , heartbeat_timeout_(0.1, 0.15)
    , term_(0)
    , has_crashed_(false)
    , has_voted_(false)
    , next_index_(nb_server + 1)
    , commit_index_(nb_server + 1)
    , log_entries_("entries_server" + std::to_string(rank) + ".log")
    , logs_to_be_commited_()
    , logger_("log_server" + std::to_string(rank) + ".log")
{
    LOG(DEBUG) << "server " << rank_ << "has PID " << getpid();
}

//------------------------------------------------------------------//
//                          Main functions                          //
//------------------------------------------------------------------//

void Server::update()
{
    auto status = mpi::available_message();

    if (status && status->MPI_TAG == MessageTag::REPL)
        return handle_repl_request(status->MPI_SOURCE);

    if (has_crashed_)
        return ignore_messages();

    if (speed_mod_ > 1)
        std::this_thread::sleep_for(
            std::chrono::milliseconds(20 * (speed_mod_ / 3)));

    if (status_ != Status::LEADER)
    {
        if (timeout_)
            return candidate();

        follower();
    }

    else
        leader();
}

bool Server::complete() const
{
    if (leader_ != rank_)
        return log_entries_.get_commit_index() == nb_request_ - 1;

    for (int i = 1; i <= nb_server_; i++)
        if (i != rank_ && commit_index_[i] != nb_request_ - 1)
            return false;

    return true;
}

//------------------------------------------------------------------//
//                            Main roles                            //
//------------------------------------------------------------------//

void Server::leader()
{
    if (heartbeat_timeout_)
        return heartbeat();

    auto status = mpi::available_message();

    if (!status)
        return;

    if (status->MPI_TAG == MessageTag::REPL)
        return handle_repl_request(status->MPI_SOURCE);

    if (status->MPI_TAG == MessageTag::CLIENT_REQUEST)
        return handle_client_request(status->MPI_SOURCE, status->MPI_TAG);

    LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
    auto recv_data =
        mpi::recv<ServerMessage>(status->MPI_SOURCE, status->MPI_TAG);

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

        timeout_.reset();
    }

    else
    {
        LOG(DEBUG) << "received message from :" << recv_data.source
                   << " with tag " << recv_data.tag;
    }
}

void Server::candidate()
{
    timeout_.reset();
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
        if (timeout_)
            return candidate();

        auto status = mpi::available_message();

        if (!status)
            continue;

        if (status->MPI_TAG == MessageTag::REPL)
            return handle_repl_request(status->MPI_SOURCE);

        if (status->MPI_TAG == MessageTag::VOTE)
        {
            LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
            auto recv_data =
                mpi::recv<ServerMessage>(status->MPI_SOURCE, status->MPI_TAG);

            // If not up to date, give up election
            if (recv_data.last_log_index > log_entries_.last_log_index()
                || recv_data.last_log_term > log_entries_.last_log_term())
            {
                LOG(INFO) << "got a reject vote from " << recv_data.source;
                timeout_.reset();

                return;
            }
            LOG(INFO) << "got a vote from " << recv_data.source;
            nb_votes++;
        }

        // TODO: handle append_entries tag
        else if (status->MPI_TAG == MessageTag::HEARTBEAT)
        {
            LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
            auto recv_data =
                mpi::recv<ServerMessage>(status->MPI_SOURCE, status->MPI_TAG);

            if (recv_data.term >= term_)
            {
                LOG(INFO) << "received heartbeat from more legitimate "
                             "leader or server that won the election: "
                          << recv_data.source;

                leader_ = recv_data.source;
                status_ = Status::FOLLOWER;

                update_term(recv_data.term);

                timeout_.reset();

                return;
            }
        }

        else if (status->MPI_TAG == MessageTag::CLIENT_REQUEST)
        {
            reject_client(status->MPI_SOURCE, status->MPI_TAG);
        }

        else
        {
            LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
            auto recv_data =
                mpi::recv<ServerMessage>(status->MPI_SOURCE, status->MPI_TAG);
            LOG(DEBUG) << "received message from :" << recv_data.source
                       << " with tag " << recv_data.tag;
        }
    }

    become_leader();
}

void Server::follower()
{
    status_ = Status::FOLLOWER;

    auto status = mpi::available_message();

    if (!status)
        return;

    LOG(DEBUG) << "available tag is " << status->MPI_TAG;
    if (status->MPI_TAG == MessageTag::REPL)
        return handle_repl_request(status->MPI_SOURCE);

    if (status->MPI_TAG == MessageTag::CLIENT_REQUEST)
        return reject_client(status->MPI_SOURCE, status->MPI_TAG);

    if (status->MPI_TAG == MessageTag::REQUEST_VOTE)
        return handle_request_vote(status->MPI_SOURCE, status->MPI_TAG);

    if (status->MPI_TAG == MessageTag::APPEND_ENTRIES)
        return handle_append_entries(status->MPI_SOURCE, status->MPI_TAG);

    if (status->MPI_TAG == MessageTag::HEARTBEAT)
    {
        LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
        auto recv_data =
            mpi::recv<ServerMessage>(status->MPI_SOURCE, status->MPI_TAG);

        LOG(INFO) << "received heartbeat from " << recv_data.source;
        leader_ = recv_data.source;

        update_commit_index(recv_data.leader_commit);

        if (term_ < recv_data.term)
            update_term(recv_data.term);

        if (recv_data.last_log_index > log_entries_.last_log_index())
            mpi::send(recv_data.source, recv_data,
                      MessageTag::REJECT_APPEND_ENTRIES);

        timeout_.reset();
    }

    else
    {
        LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
        auto recv_data =
            mpi::recv<ServerMessage>(status->MPI_SOURCE, status->MPI_TAG);

        LOG(DEBUG) << "received message from :" << recv_data.source
                   << " with tag " << recv_data.tag;
    }
}

//------------------------------------------------------------------//
//                         Leader functions                         //
//------------------------------------------------------------------//

void Server::heartbeat()
{
    heartbeat_timeout_.reset();
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

void Server::init_next_index()
{
    int last_index = log_entries_.last_log_index();
    for (int i = 1; i <= nb_server_; i++)
        next_index_[i] = last_index + 1;
}

void Server::init_commit_index()
{
    for (int i = 1; i <= nb_server_; i++)
        commit_index_[i] = -1;
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

//------------------------------------------------------------------//
//                        Follower functions                        //
//------------------------------------------------------------------//

void Server::reject_client(int src, int tag)
{
    LOG(DEBUG) << "recv from client at " << __FILE__ << ":" << __LINE__;
    auto recv_data = mpi::recv<Client::ClientMessage>(src, tag);
    auto message = init_message();
    mpi::send(recv_data.source, message, MessageTag::REJECT);
}

void Server::update_commit_index(int index)
{
    while (index > log_entries_.get_commit_index())
    {
        if (!log_entries_.commit_next_entry())
            break;
        LOG(INFO) << "commited log number: "
                  << log_entries_.get_commit_index() + 1;
    }

    auto message = init_message();
    // use commit index as log index to avoid update on logs_to_be_commited_ map
    message.log_index = log_entries_.get_commit_index();
    mpi::send(leader_, message, MessageTag::ACCEPT_APPEND_ENTRIES);
}

//------------------------------------------------------------------//
//                            Elections                             //
//------------------------------------------------------------------//

void Server::vote(int server)
{
    LOG(INFO) << "voting for " << server;
    timeout_.reset();

    has_voted_ = true;

    auto message = init_message();
    mpi::send(server, message, MessageTag::VOTE);
}

void Server::become_leader()
{
    heartbeat();
    init_next_index();
    init_commit_index();
    status_ = Status::LEADER;
    leader_ = rank_;
    LOG(INFO) << "become the leader";
}

//------------------------------------------------------------------//
//                            Utilities                             //
//------------------------------------------------------------------//

void Server::update_term()
{
    update_term(term_ + 1);
}

void Server::update_term(int term)
{
    if (term >= term_)
    {
        term_ = term;
        has_voted_ = false;
    }

    LOG(INFO) << "current term: " << term_;
}

void Server::append_entries(int term, int client_id, int data, int request_id)
{
    LOG(INFO) << "AppendEntries: index = " << log_entries_.size()
              << ", term: " << term << ", client: " << client_id
              << ", entry: " << data << ", request_id " << request_id;

    log_entries_.append_entry(term, client_id, data, request_id);
}

Server::ServerMessage Server::init_message(int entry)
{
    ServerMessage message;

    message.term = term_;
    message.last_log_index = log_entries_.last_log_index();

    message.last_log_term = log_entries_.last_log_term();

    message.commit_index = log_entries_.get_commit_index();

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

void Server::ignore_messages()
{
    while (auto status = mpi::available_message())
    {
        if (status->MPI_TAG == MessageTag::REPL)
            break;

        if (status->MPI_TAG == MessageTag::CLIENT_REQUEST)
        {
            LOG(DEBUG) << "recv from client at " << __FILE__ << ":" << __LINE__;
            mpi::recv<Client::ClientMessage>(status->MPI_SOURCE,
                                             status->MPI_TAG);
        }
        else
        {
            LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
            mpi::recv<ServerMessage>(status->MPI_SOURCE, status->MPI_TAG);
        }
    }
}

//------------------------------------------------------------------//
//                         Message handlers                         //
//------------------------------------------------------------------//

void Server::handle_accept_append_entry(const ServerMessage& recv_data)
{
    LOG(INFO) << "received acknowledge for log :" << recv_data.log_index
              << " from server " << recv_data.source;

    next_index_[recv_data.source] = recv_data.log_index + 1;
    commit_index_[recv_data.source] = recv_data.commit_index;

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

    if (next_index_[recv_data.source]-- <= 0)
        next_index_[recv_data.source] = 0;
}

void Server::handle_append_entries(int src, int tag)
{
    LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
    auto recv_data = mpi::recv<ServerMessage>(src, tag);

    if (recv_data.term < term_)
    {
        LOG(INFO) << "rejecting append entries term:" << recv_data.term << "|"
                  << term_;

        return mpi::send(leader_, recv_data, MessageTag::REJECT_APPEND_ENTRIES);
    }

    leader_ = recv_data.source;
    timeout_.reset();

    if (recv_data.last_log_index > log_entries_.last_log_index()
        || recv_data.last_log_term > log_entries_.last_log_term())
    {
        LOG(INFO) << "rejecting append entries term:" << recv_data.term << "|"
                  << term_ << " last log_index : " << recv_data.last_log_index
                  << "|" << log_entries_.last_log_index()
                  << " log term:" << recv_data.last_log_term << "|"
                  << log_entries_.last_log_term();

        return mpi::send(leader_, recv_data, MessageTag::REJECT_APPEND_ENTRIES);
    }

    // last_log_index == prev_log_index
    if (log_entries_.last_log_index() > recv_data.last_log_index
        && recv_data.term != log_entries_[recv_data.last_log_index + 1].term)
    {
        log_entries_.delete_from_index(recv_data.last_log_index + 1);
    }

    append_entries(recv_data.term, recv_data.client_id, recv_data.entry,
                   recv_data.request_id);

    auto message = init_message();
    message.log_index = recv_data.last_log_index + 1;
    message.client_id = recv_data.client_id;

    update_commit_index(recv_data.leader_commit);
    update_term(recv_data.term);

    mpi::send(leader_, message, MessageTag::ACCEPT_APPEND_ENTRIES);
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
}

void Server::handle_request_vote(int src, int tag)
{
    LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
    auto recv_data = mpi::recv<ServerMessage>(src, tag);

    if (term_ <= recv_data.term)
    {
        update_term(recv_data.term);

        if (log_entries_.last_log_index() <= recv_data.last_log_index
            && log_entries_.last_log_term() <= recv_data.last_log_term)
        {
            vote(recv_data.source);
        }
        else
        {
            LOG(INFO) << "rejecting vote for " << src;
            auto message = init_message();
            mpi::send(src, message, MessageTag::VOTE);
        }
    }
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

            std::cout << "Commit: ";
            for (int i = 1; i <= nb_server_; i++)
            {
                std::cout << commit_index_[i] << " ";
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
    if (message.order == Repl::Order::RECOVERY && has_crashed_)
    {
        LOG(DEBUG) << "recover";
        has_crashed_ = false;
        status_ = Status::FOLLOWER;
        timeout_.reset();
    }

    timeout_.speed_mod = speed_mod_;
    heartbeat_timeout_.speed_mod = speed_mod_;
}
