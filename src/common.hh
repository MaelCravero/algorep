#pragma once

/// Message type passed as tag
enum MessageTag
{
    APPEND_ENTRIES = 0,
    HEARTBEAT,
    REQUEST_VOTE,
    VOTE,
    CLIENT_REQUEST,
    REJECT,
    ACKNOWLEDGE,
    ACKNOWLEDGE_APPEND_ENTRIES,
};

struct Message
{
    int term;

    int last_log_index;
    int last_log_term;

    int entry;

    int client_id;
    int log_index;

    int leader_id;
    int leader_commit;

    int source;
    int tag;
};

/// Various common types
using rank = int;
