#pragma once

/// Message type passed as tag
enum MessageTag
{
    APPEND_ENTRIES = 0,
    REQUEST_VOTE,
    VOTE,
    CLIENT_REQUEST,
    CLIENT_REQUEST_RESPONSE,
    APPEND_ENTRIES_RESPONSE,
    REPL,
};

/// Various common types
using rank = int;
