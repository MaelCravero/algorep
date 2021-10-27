#pragma once

#include <chrono>
#include <map>
#include <mpi.h>
#include <mpi/mpi.hh>
#include <string>
#include <vector>

#include "client.hh"
#include "common.hh"
#include "utils/log_entries.hh"
#include "utils/logger.hh"

class Server
{
public:
    enum class Status
    {
        FOLLOWER,
        CANDIDATE,
        LEADER,
    };

    enum class AppendEntriesStatus
    {
        SUCCESS,
        FAILURE,
    };

    struct ServerMessage : public Message
    {
        int term;

        int last_log_index;
        int last_log_term;

        int entry;

        int client_id;
        int log_index;

        int leader_id;
        int leader_commit;
    };

    using timestamp = std::chrono::duration<double>;

    Server(rank rank, int nb_server);

    /// Update server, if timeout is reached then start an election
    void update();

    int get_log_number() const;

    /// Accessors
    /// \{
    rank leader_get() const
    {
        return leader_;
    }
    /// \}

protected:
    /// Process round depending on status
    /// \{
    void leader();
    void candidate();
    void follower();

    void vote(int server);
    void append_entries(int term, int client_id, int data);
    /// \}

private:
    void start_election();
    void request_vote();

    void heartbeat();
    void reset_timeout();
    void reset_heartbeat_timeout();

    void become_leader();

    void update_term();
    void update_term(int term);

    // Send to all other servers
    void broadcast(const ServerMessage& message, int tag);

    ServerMessage init_message(int entry = 0);

    void handle_client_request();
    void handle_repl_request();
    void handle_ack_append_entry(const ServerMessage& recv_data);

    void commit_entry(int log_index, int client_id);

    void ignore_messages();

private:
    /// Status of the server
    Status status_;

    /// Rank of the server
    rank rank_;

    /// Size of the network
    int nb_server_;

    /// Rank of the leader
    rank leader_;

    int speed_mod_;

    /// Timestamp of the timeout
    timestamp timeout_;

    /// Timestamp of the timeout
    timestamp heartbeat_timeout_;

    /// Current term
    int term_;

    /// Crash status
    bool has_crashed_;

    /// index of the next log entry to send to that server
    std::vector<int> next_index_;

    /// index of highest log entry known to be replicated on server
    std::vector<int> match_index_;

    /// Log entries
    utils::LogEntries log_entries_;

    // Map log index on nb_acknowledge
    std::map<int, int> logs_to_be_commited_;

    utils::Logger logger_;
};
