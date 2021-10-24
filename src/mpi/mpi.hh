#pragma once

#include <cstring>
#include <mpi.h>
#include <optional>
#include <string>

#include "common.hh"

namespace mpi
{
    using status = MPI_Status;

    inline void send(rank dst, const Message& message, int tag)
    {
        int buffer[8] = {message.term,          message.last_log_index,
                         message.last_log_term, message.entry,
                         message.client_id,     message.log_index,
                         message.leader_id,     message.leader_commit};

        MPI_Bsend(buffer, 8, MPI_INT, dst, tag, MPI_COMM_WORLD);
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
