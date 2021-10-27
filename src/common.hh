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
    ACCEPT_APPEND_ENTRIES,
    REJECT_APPEND_ENTRIES,
    REPL,
};

struct Message
{
    int source;
    int tag;
};

/// Various common types
using rank = int;
