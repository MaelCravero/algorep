#pragma once

#include <chrono>
#include <mpi.h>
#include <mpi/mpi.hh>
#include <string>
#include <vector>

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

    using timestamp = std::chrono::duration<double>;

    Server(rank rank, int size);

    /// Update server, if timeout is reached then start an election
    void update();

    void start_election();
    void request_vote();

    void heartbeat();
    void reset_timeout();
    void reset_leader_timeout();

    AppendEntriesStatus append_entries(std::string log);

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
    void become_leader();

    void update_term();
    void update_term(int term);

    // Send to all other servers
    void broadcast(const Message& message, int tag);

    Message init_message(int entry = 0);

    void handle_client_request(const Message& recv_data);
    void handle_ack_append_entry(const Message& recv_data);

private:
    /// Status of the server
    Status status_;

    /// Rank of the server
    rank rank_;

    /// Size of the network
    /// FIXME: should be number of servers
    int size_;

    /// Rank of the leader
    rank leader_;

    /// Timestamp of the timeout
    timestamp timeout_;

    /// Timestamp of the timeout
    timestamp heartbeat_timeout_;

    /// Current term
    int term_;

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
