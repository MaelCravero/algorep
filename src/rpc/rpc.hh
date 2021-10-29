#pragma once

#include <optional>

#include "common.hh"
#include "utils/bounded_string.hh"

namespace rpc
{
    using command_t = utils::bounded_string<64>;

    struct ClientRequest
    {
        rank source;
        unsigned id;
        command_t command;

        inline bool operator==(const ClientRequest& other)
        {
            return source == other.source && id == other.id
                && command == other.command;
        }
    };

    struct AppendEntries
    {
        rank source;
        int term;
        rank leader;
        int prev_log_index;
        int prev_log_term;
        std::optional<ClientRequest> entry;
        int leader_commit;
    };

    struct RequestVote
    {
        int term;
        rank candidate;
        int last_log_index;
        int last_log_term;
    };

    struct RequestVoteResponse
    {
        rank source;
        bool value;
    };

    struct ClientRequestResponse
    {
        rank source;
        bool value;
        rank leader;
    };

    struct AppendEntriesResponse
    {
        rank source;
        bool value;
        int log_index;
        int commit_index;
    };

    struct REPL
    {
        enum class Order : char
        {
            SPEED = 's',
            CRASH = 'c',
            BEGIN = 'b',
            RECOVERY = 'r',
            PRINT = 'p',
        };

        Order order;
        int speed_level;
    };

} // namespace rpc
