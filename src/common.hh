#pragma once

/// Message type passed as tag
enum MessageType
{
    VOTE,
    HEARTBEAT,
    ACK,
    COMMIT,
};

/// Various common types
using rank = int;
