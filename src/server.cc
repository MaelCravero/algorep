#include "server.hh"

#include <assert.h>
#include <iostream>
#include <unistd.h>

#include "repl.hh"
#include "rpc/rpc.hh"

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
    , nb_vote_(0)
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
        utils::sleep_for_ms(20 * (speed_mod_ / 3));

    if (status_ == Status::LEADER)
        leader();

    else if (timeout_)
        start_election();

    else if (status_ == Status::CANDIDATE)
        candidate();

    else
        follower();
}

bool Server::complete() const
{
    if (leader_ != rank_)
        return log_entries_.get_commit_index() >= nb_request_ - 1;

    for (int i = 1; i <= nb_server_; i++)
        if (i != rank_ && commit_index_[i] < nb_request_ - 1)
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

    if (status->MPI_TAG == MessageTag::APPEND_ENTRIES_RESPONSE)
    {
        LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;

        auto recv_data = mpi::recv<rpc::AppendEntriesResponse>(
            status->MPI_SOURCE, status->MPI_TAG);

        handle_append_entry_response(recv_data);
    }

    else if (status->MPI_TAG == MessageTag::APPEND_ENTRIES)
        handle_append_entries(status->MPI_SOURCE, status->MPI_TAG);

    else
        drop_message(status->MPI_SOURCE, status->MPI_TAG);
}

void Server::candidate()
{
    // on timeout start a new election
    if (timeout_)
        return start_election();

    auto status = mpi::available_message();

    if (!status)
        return;

    if (status->MPI_TAG == MessageTag::REPL)
        return handle_repl_request(status->MPI_SOURCE);

    if (status->MPI_TAG == MessageTag::VOTE)
    {
        LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
        auto recv_data = mpi::recv<rpc::RequestVoteResponse>(status->MPI_SOURCE,
                                                             status->MPI_TAG);

        // If not up to date, give up election
        if (!recv_data.value)
        {
            LOG(INFO) << "got a reject vote from " << recv_data.source;
            timeout_.reset();
            status_ = Status::FOLLOWER;

            return;
        }

        LOG(INFO) << "got a vote from " << recv_data.source;
        nb_vote_++;
    }

    else if (status->MPI_TAG == MessageTag::APPEND_ENTRIES)
        handle_append_entries(status->MPI_SOURCE, status->MPI_TAG);

    else if (status->MPI_TAG == MessageTag::CLIENT_REQUEST)
        reject_client(status->MPI_SOURCE, status->MPI_TAG);

    else if (status->MPI_TAG == MessageTag::REQUEST_VOTE)
        handle_request_vote(status->MPI_SOURCE, status->MPI_TAG);

    else
        drop_message(status->MPI_SOURCE, status->MPI_TAG);

    // If got a majority of votes, become the leader
    if (nb_vote_ > nb_server_ / 2)
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

    else
        drop_message(status->MPI_SOURCE, status->MPI_TAG);
}

//------------------------------------------------------------------//
//                         Leader functions                         //
//------------------------------------------------------------------//

void Server::heartbeat()
{
    heartbeat_timeout_.reset();

    for (int i = 1; i <= nb_server_; i++)
    {
        if (i == rank_)
            continue;

        rpc::AppendEntries message{
            rank_, term_, leader_, -1, -1, {}, log_entries_.get_commit_index()};

        message.prev_log_index = next_index_[i] - 1;
        message.prev_log_term =
            next_index_[i] - 1 < 0 ? -1 : log_entries_[next_index_[i] - 1].term;

        if (next_index_[i] > log_entries_.last_log_index())
        {
            LOG(INFO) << "server: " << i << " is up to date, next_index is "
                      << next_index_[i];

            message.entry = {};
        }

        else
        {
            auto next_entry = log_entries_[next_index_[i]];
            message.entry = next_entry.data;

            LOG(INFO) << "Sending append entries to " << i
                      << ", client_id: " << next_entry.data.source
                      << ", request_id: " << next_entry.data.id
                      << ", entry_command: " << message.entry->command
                      << ", term: " << term_
                      << ", prev_log_index: " << message.prev_log_index
                      << ", prev_log_term: " << message.prev_log_term;
        }

        mpi::send(i, message, MessageTag::APPEND_ENTRIES);
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
    rpc::ClientRequestResponse message{rank_, true, leader_};
    mpi::send(client_id, message, MessageTag::CLIENT_REQUEST_RESPONSE);

    LOG(INFO) << "Notify client " << client_id << " for request " << log_index;
}

//------------------------------------------------------------------//
//                        Follower functions                        //
//------------------------------------------------------------------//

void Server::reject_client(int src, int tag)
{
    LOG(DEBUG) << "recv from client at " << __FILE__ << ":" << __LINE__;
    auto recv_data = mpi::recv<rpc::ClientRequest>(src, tag);

    rpc::ClientRequestResponse message{rank_, false, leader_};
    mpi::send(recv_data.source, message, MessageTag::CLIENT_REQUEST_RESPONSE);
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
}

//------------------------------------------------------------------//
//                            Elections                             //
//------------------------------------------------------------------//

void Server::vote(int server)
{
    LOG(INFO) << "voting for " << server;
    timeout_.reset();

    rpc::RequestVoteResponse message{rank_, true};
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

    // Only keep what has been commited in the log
    log_entries_.delete_from_index(log_entries_.get_commit_index() + 1);
}

void Server::start_election()
{
    LOG(INFO) << "Become candidate";
    status_ = Status::CANDIDATE;
    timeout_.reset();
    update_term();

    // Ask for votes
    auto message = rpc::RequestVote{term_, rank_, log_entries_.last_log_index(),
                                    log_entries_.last_log_term()};

    broadcast(message, MessageTag::REQUEST_VOTE);
    nb_vote_ = 1; // Vote for himself

    candidate();
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
        term_ = term;

    LOG(INFO) << "current term: " << term_;
}

void Server::append_entries(int term, rpc::ClientRequest data)
{
    LOG(INFO) << "AppendEntries: index = " << log_entries_.size()
              << ", term: " << term << ", client: " << data.source
              << ", command: " << data.command << ", id " << data.id;

    log_entries_.append_entry(term, data);
}

void Server::broadcast(const rpc::RequestVote& message, int tag)
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

        drop_message(status->MPI_SOURCE, status->MPI_TAG);
    }
}

void Server::drop_message(int src, int tag)
{
    LOG(WARN) << "dropping message from :" << src << " with tag " << tag;

    // AppendEntries is the biggest RPC
    mpi::recv<rpc::AppendEntries>(src, tag);
}

//------------------------------------------------------------------//
//                         Message handlers                         //
//------------------------------------------------------------------//

void Server::handle_append_entry_response(
    const rpc::AppendEntriesResponse& recv_data)
{
    LOG(INFO) << "received append_entry_response from server :"
              << recv_data.source << ", added log: " << std::boolalpha
              << recv_data.value;

    next_index_[recv_data.source] = recv_data.log_index + 1;
    commit_index_[recv_data.source] = recv_data.commit_index;

    LOG(INFO) << "server :" << recv_data.source
              << " next index: " << next_index_[recv_data.source]
              << " commit index: " << commit_index_[recv_data.source];

    if (recv_data.value && logs_to_be_commited_.contains(recv_data.log_index))
    {
        auto nb_ack = ++logs_to_be_commited_[recv_data.log_index];

        if (nb_ack > nb_server_ / 2
            && log_entries_.get_commit_index() == recv_data.log_index - 1)
        {
            commit_entry(recv_data.log_index,
                         log_entries_[recv_data.log_index].data.source);

            int i = recv_data.log_index + 1;
            while (logs_to_be_commited_.contains(i)
                   && logs_to_be_commited_[i] > nb_server_ / 2)
            {
                commit_entry(i, log_entries_[i].data.source);
                i++;
            }
        }
    }
}

void Server::handle_append_entries(int src, int tag)
{
    LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
    auto recv_data = mpi::recv<rpc::AppendEntries>(src, tag);

    rpc::AppendEntriesResponse message{rank_, false,
                                       log_entries_.last_log_index(),
                                       log_entries_.get_commit_index()};

    if (recv_data.term < term_
        || log_entries_.get_commit_index() > recv_data.leader_commit)
    {
        LOG(INFO) << "rejecting append entries term:" << recv_data.term << "|"
                  << term_;

        update_term(recv_data.term);
        mpi::send(leader_, message, MessageTag::APPEND_ENTRIES_RESPONSE);
        return;
    }

    leader_ = recv_data.source;
    status_ = Status::FOLLOWER;
    timeout_.reset();

    if (recv_data.prev_log_index > log_entries_.last_log_index()
        || recv_data.prev_log_term > log_entries_.last_log_term())
    {
        LOG(INFO) << "rejecting append entries term:" << recv_data.term << "|"
                  << term_ << " prev log_index : " << recv_data.prev_log_index
                  << "|" << log_entries_.last_log_index()
                  << " log term:" << recv_data.prev_log_term << "|"
                  << log_entries_.last_log_term();

        if (recv_data.prev_log_term > log_entries_.last_log_term())
        {
            log_entries_.delete_from_index(recv_data.prev_log_index);
            message.log_index = log_entries_.last_log_index();
        }

        return mpi::send(leader_, message, MessageTag::APPEND_ENTRIES_RESPONSE);
    }

    // last_log_index == prev_log_index
    if (log_entries_.last_log_index() > recv_data.prev_log_index
        && recv_data.term != log_entries_[recv_data.prev_log_index + 1].term)
    {
        LOG(INFO) << "delete from index " << recv_data.prev_log_index + 1;

        log_entries_.delete_from_index(recv_data.prev_log_index + 1);
    }

    // No entry, message is just a heartbeat
    if (recv_data.entry)
        append_entries(recv_data.term, *recv_data.entry);

    update_commit_index(recv_data.leader_commit);
    update_term(recv_data.term);

    message = {rank_, recv_data.entry.has_value(),
               log_entries_.last_log_index(), log_entries_.get_commit_index()};

    LOG(INFO) << "accept append entries " << message.commit_index << "/"
              << message.log_index;

    mpi::send(leader_, message, MessageTag::APPEND_ENTRIES_RESPONSE);
}

void Server::handle_client_request(int src, int tag)
{
    LOG(DEBUG) << "recv from client at " << __FILE__ << ":" << __LINE__;

    auto recv_data = mpi::recv<rpc::ClientRequest>(src, tag);

    LOG(INFO) << "received message from client:" << recv_data.source;

    append_entries(term_, recv_data);

    logs_to_be_commited_.try_emplace(log_entries_.last_log_index(), 1);
}

void Server::handle_request_vote(int src, int tag)
{
    LOG(DEBUG) << "recv from server at " << __FILE__ << ":" << __LINE__;
    auto recv_data = mpi::recv<rpc::RequestVote>(src, tag);

    if (term_ <= recv_data.term)
    {
        update_term(recv_data.term);

        if (log_entries_.last_log_index() <= recv_data.last_log_index
            && log_entries_.last_log_term() <= recv_data.last_log_term)
        {
            vote(recv_data.candidate);
        }
        else
        {
            LOG(INFO) << "rejecting vote for " << src;

            rpc::RequestVoteResponse message{rank_, false};
            mpi::send(src, message, MessageTag::VOTE);
            start_election();
        }
    }
}

void Server::handle_repl_request(int src)
{
    LOG(DEBUG) << "recv from repl at " << __FILE__ << ":" << __LINE__;
    auto message = mpi::recv<rpc::Repl>(src, MessageTag::REPL);

    if (message.order == Repl::Order::PRINT)
    {
        utils::sleep_for_ms(30 * rank_);

        std::cout << "Server: " << rank_ << "/" << nb_server_ << "\n";
        std::cout << "   PID: " << getpid() << "\n";
        std::cout << " Speed: " << speed_mod_ << "\n";
        std::cout << " Crash: " << std::boolalpha << has_crashed_ << "\n";
        std::cout << "Leader: " << leader_ << "\n";

        if (leader_ == rank_)
        {
            std::cout << "NxtIdx: ";
            for (int i = 1; i <= nb_server_; i++)
                std::cout << next_index_[i] << " ";
            std::cout << "\n";

            std::cout << "Commit: ";
            for (int i = 1; i <= nb_server_; i++)
                std::cout << commit_index_[i] << " ";
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

        // Only keep what has been commited in the log
        log_entries_.delete_from_index(log_entries_.get_commit_index() + 1);
        timeout_.reset();
    }

    timeout_.speed_mod = speed_mod_;
    heartbeat_timeout_.speed_mod = speed_mod_;
}
