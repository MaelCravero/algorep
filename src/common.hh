#pragma once

#include <string>

/// Message type passed as tag
enum MessageTag
{
    APPEND_ENTRIES = 0,
    APPEND_ENTRIES_RESPONSE,
    REQUEST_VOTE,
    VOTE,
    CLIENT_REQUEST,
    CLIENT_REQUEST_RESPONSE,
    REPL,
};

inline std::string tag_to_str(int tag)
{
    switch (tag)
    {
    case MessageTag::APPEND_ENTRIES:
        return "APPEND_ENTRIES";
    case MessageTag::APPEND_ENTRIES_RESPONSE:
        return "APPEND_ENTRIES_RESPONSE";
    case MessageTag::REQUEST_VOTE:
        return "REQUEST_VOTE";
    case MessageTag::VOTE:
        return "VOTE";
    case MessageTag::CLIENT_REQUEST:
        return "CLIENT_REQUEST";
    case MessageTag::CLIENT_REQUEST_RESPONSE:
        return "CLIENT_REQUEST_RESPONSE";
    case MessageTag::REPL:
        return "REPL";
    default:
        return "";
    }
}

/// follower rank:monrank_
using rank = int;
