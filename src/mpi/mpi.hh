#pragma once

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

    struct RecvData
    {
        int message;
        int source;
        int tag;
    };

    inline void send(rank dst, int message, int tag)
    {
        // FIXME: Should we do something with this request ?
        request request = MPI_REQUEST_NULL;

        // Shouldn't message be available until it gets received ?
        // I don't know why this works while message is on the stack and is
        // dealocated right away
        MPI_Isend(&message, 1, MPI_INT, dst, tag, MPI_COMM_WORLD, &request);
    }

    inline RecvData recv(int src = MPI_ANY_SOURCE, int tag = MPI_ANY_TAG)
    {
        int message;
        status status;

        MPI_Recv(&message, 1, MPI_INT, src, tag, MPI_COMM_WORLD, &status);

        return {message, status.MPI_SOURCE, status.MPI_TAG};
    }

    inline bool available_message(int src = MPI_ANY_SOURCE,
                                  int tag = MPI_ANY_TAG)
    {
        int flag;
        MPI_Iprobe(src, tag, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);

        return flag;
    }

} // namespace mpi
