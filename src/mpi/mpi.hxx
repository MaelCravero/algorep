#pragma once

#include "mpi/mpi.hh"

namespace mpi
{
    template <typename M>
    inline void Mpi::send(rank dst, const M& message, int tag)
    {
        // Convert the message to a char* buffer to send it the same way no
        // matter the type of Message
        const char* buffer = reinterpret_cast<const char*>(&message);

        MPI_Bsend(buffer, sizeof(M), MPI_CHAR, dst, tag, MPI_COMM_WORLD);
        send_stats_[tag]++;
    }

    template <typename M>
    inline M Mpi::recv(int src, int tag)
    {
        char buffer[sizeof(M)];
        status status;

        MPI_Recv(buffer, sizeof(M), MPI_CHAR, src, tag, MPI_COMM_WORLD,
                 &status);

        // Convert the char* buffer back to Message type and add source and
        // tag info
        M message = *reinterpret_cast<M*>(buffer);

        recv_stats_[tag]++;

        return message;
    }

    inline std::optional<Mpi::status> Mpi::available_message(int src, int tag)
    {
        int flag;
        status status;
        MPI_Iprobe(src, tag, MPI_COMM_WORLD, &flag, &status);

        if (!flag)
            return {};
        return {status};
    }

    inline Mpi::stats_pair_type Mpi::get_stats() const
    {
        return {send_stats_, recv_stats_};
    }
} // namespace mpi
