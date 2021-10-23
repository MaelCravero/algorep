#pragma once

#include <mpi.h>
#include <optional>
#include <string>

#include "common.hh"

namespace mpi
{
    using request = MPI_Request;

    inline void send(rank dst, int message, int tag)
    {
        // FIXME: Should we do something with this request ?
        request request = MPI_REQUEST_NULL;

        // Shouldn't message be available until it gets received ?
        // I don't know why this works while message is on the stack and is
        // dealocated right away
        MPI_Isend(&message, 1, MPI_INT, dst, tag, MPI_COMM_WORLD, &request);
    }

    inline int recv(int src = MPI_ANY_SOURCE, int tag = MPI_ANY_TAG)
    {
        int message;

        MPI_Recv(&message, 1, MPI_INT, src, tag, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);

        return message;
    }

} // namespace mpi
