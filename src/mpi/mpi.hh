#pragma once

#include <cstring>
#include <mpi.h>
#include <optional>
#include <string>

#include "common.hh"

namespace mpi
{
    using request = MPI_Request;
    using status = MPI_Status;

    enum MessageTag
    {
        APPEND_ENTRIES = 0,
        HEARTBEAT,
        REQUEST_VOTE,
        VOTE,
        CLIENT_REQUEST,
        REJECT,
        ACKNOWLEDGE,
    };

    struct Message
    {
        int term;

        int last_log_index;
        int last_log_term;

        int prev_log_index;
        int prev_log_term;

        int entry;

        int leader_id;
        int leader_commit;

        int source;
        int tag;
    };

    inline void send(rank dst, const Message& message, int tag)
    {
        // FIXME: Should we do something with this request ?
        request request = MPI_REQUEST_NULL;

        int buffer[8] = {message.term,          message.last_log_index,
                         message.last_log_term, message.prev_log_index,
                         message.prev_log_term, message.entry,
                         message.leader_id,     message.leader_commit};

        // Shouldn't message be available until it gets received ?
        // I don't know why this works while message is on the stack and is
        // dealocated right away
        MPI_Isend(buffer, 8, MPI_INT, dst, tag, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, MPI_STATUS_IGNORE);
    }

    inline Message recv(int src = MPI_ANY_SOURCE, int tag = MPI_ANY_TAG)
    {
        int buffer[8];
        status status;

        MPI_Recv(buffer, 8, MPI_INT, src, tag, MPI_COMM_WORLD, &status);

        Message message = {
            buffer[0], buffer[1], buffer[2], buffer[3],         buffer[4],
            buffer[5], buffer[6], buffer[7], status.MPI_SOURCE, status.MPI_TAG};

        return message;
    }

    inline bool available_message(int src = MPI_ANY_SOURCE,
                                  int tag = MPI_ANY_TAG)
    {
        int flag;
        MPI_Iprobe(src, tag, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);

        return flag;
    }

} // namespace mpi
