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

    using timestamp = std::chrono::duration<double>;

    Server(rank rank, int size);

    /// Update server, if timeout is reached then start an election
    void update();

    void start_election();
    void request_vote();

    void heartbeat();
    void reset_timeout();

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
    /// Current term
    size_t term_;
    /// ? FIXME
    std::vector<size_t> next_index_;
};
