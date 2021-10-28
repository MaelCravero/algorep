#pragma once

#include <cstring>
#include <mpi.h>
#include <optional>
#include <string>

#include "common.hh"

namespace mpi
{
    using status = MPI_Status;

    template <typename M>
    inline void send(rank dst, const M& message, int tag)
    {
        // Convert the message to a char* buffer to send it the same way no
        // matter the type of Message
        const char* buffer = reinterpret_cast<const char*>(&message);

        MPI_Bsend(buffer, sizeof(M), MPI_CHAR, dst, tag, MPI_COMM_WORLD);
    }

    template <typename M>
    inline M recv(int src = MPI_ANY_SOURCE, int tag = MPI_ANY_TAG)
    {
        char buffer[sizeof(M)];
        status status;

        MPI_Recv(buffer, sizeof(M), MPI_CHAR, src, tag, MPI_COMM_WORLD,
                 &status);

        // Convert the char* buffer back to Message type and add source and tag
        // info
        M message = *reinterpret_cast<M*>(buffer);

        message.source = status.MPI_SOURCE;
        message.tag = status.MPI_TAG;

        return message;
    }

    inline std::optional<status> available_message(int src = MPI_ANY_SOURCE,
                                                   int tag = MPI_ANY_TAG)
    {
        int flag;
        status status;
        MPI_Iprobe(src, tag, MPI_COMM_WORLD, &flag, &status);

        if (!flag)
            return {};
        return {status};
    }

} // namespace mpi
