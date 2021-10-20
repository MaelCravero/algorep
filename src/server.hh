#pragma once

#include <chrono>
#include <mpi.h>
#include <string>
#include <vector>

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

class Server
{
public:
    Server(int rank, int size);
    void start_election();

    void heartbeat();

    AppendEntriesStatus append_entries(std::string log);

    void request_vote();

    // Update server, if timeout is reached then start an election
    void update();

    int get_leader_rank();

private:
    Status status_;
    int rank_;
    int size_;

    int leader_rank_;

    std::vector<size_t> next_index_;

    double election_timeout_;
    double election_time_remaining_;

    std::chrono::duration<double> previous_time_;
    size_t term_;
};

void server(int rank, int size);
