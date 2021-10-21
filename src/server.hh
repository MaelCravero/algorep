#pragma once

#include <chrono>
#include <mpi.h>
#include <string>
#include <vector>

#include "common.hh"

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

    enum MessageTag
    {
        APPEND_ENTRIES = 0,
        HEARTBEAT,
        REQUEST_VOTE,
        VOTE,
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
    void become_leader();

    /// Process round depending on status
    /// \{
    void leader();
    void candidate();
    void follower();
    /// \}

private:
    /// Status of the server
    Status status_;

    /// Rank of the server
    rank rank_;

    /// Size of the network
    int size_;

    /// Rank of the leader
    rank leader_;

    /// Timestamp of the timeout
    timestamp timeout_;

    /// Timestamp of the timeout
    timestamp heartbeat_timeout_;

    /// Current term
    int term_;

    /// Last voted term
    int last_voted_term_;

    /// Next term of each node in the network
    std::vector<int> next_index_;
};
