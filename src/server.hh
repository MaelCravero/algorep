#pragma once

#include <map>
#include <mpi.h>
#include <mpi/mpi.hh>
#include <string>
#include <vector>

#include "client.hh"
#include "common.hh"
#include "utils/log_entries.hh"
#include "utils/logger.hh"
#include "utils/time.hh"

class Server
{
public:
    enum class Status
    {
        FOLLOWER,
        CANDIDATE,
        LEADER,
    };

    Server(rank rank, int nb_server);
    ~Server();

    /// Main functions
    /// \{
    /// Update server, if timeout is reached then start an election
    void update();

    /// Has the system logged all client requests
    bool complete() const;

    /// Get stats about the number of messages sent and received
    void write_stats() const;
    /// \}

private:
    /// Process round depending on status
    /// \{
    void leader();
    void candidate();
    void follower();
    /// \}

    /// Leader
    /// \{
    void heartbeat();
    void init_next_index();
    void init_commit_index();
    // Add entry to commit log
    void commit_entry(int log_index, int client_id);
    /// \}

    /// Follower
    /// \{
    void reject_client(int src, int tag);
    void update_commit_index(int index);
    /// \}

    /// Elections
    /// \{
    void vote(int server);
    void become_leader();
    void start_election();
    /// \}

    /// Utilities
    /// \{
    void update_term();
    void update_term(int term);
    void append_entries(int term, rpc::ClientRequest data);
    void broadcast(const rpc::RequestVote& message, int tag);
    // Ignore messages if crashed
    void ignore_messages();
    // Drop incoming message
    void drop_message(int src, int tag);
    /// \}

    /// Handlers
    /// \{
    void
    handle_append_entry_response(const rpc::AppendEntriesResponse& recv_data);
    void handle_append_entries(int src, int tag);
    void handle_client_request(int src, int tag);
    void handle_request_vote(int src, int tag);
    void handle_repl_request(int src);
    /// \}

private:
    /// Status of the server
    Status status_;

    /// Rank of the server
    rank rank_;

    /// Size of the network
    int nb_server_;

    /// Rank of the leader
    rank leader_;

    /// Speed slow modifier
    int speed_mod_;

    /// Timestamp of the timeout
    utils::Timeout timeout_;

    /// Timestamp of the timeout
    utils::Timeout heartbeat_timeout_;

    /// Current term
    int term_;

    /// Crash status
    bool has_crashed_;

    /// Whether the process should stop
    bool stop_;

    /// number of vote in current election
    int nb_vote_;

    /// index of the next log entry to send to that server
    std::vector<int> next_index_;

    /// commit index on each server
    std::vector<int> commit_index_;

    /// Log entries
    utils::LogEntries log_entries_;

    // Map log index on nb_acknowledge
    std::map<int, int> logs_to_be_commited_;

    utils::Logger logger_;
    mpi::Mpi mpi_;
};
