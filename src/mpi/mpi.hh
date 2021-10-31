#pragma once

#include <cstring>
#include <iostream>
#include <map>
#include <mpi.h>
#include <optional>
#include <string>

#include "common.hh"
#include "rpc/rpc.hh"

namespace mpi
{
    class Mpi
    {
    public:
        using status = MPI_Status;

        using msg_stats_map_type = std::map<int, std::size_t>;
        using stats_pair_type =
            std::pair<const msg_stats_map_type&, const msg_stats_map_type&>;

        Mpi() = default;

        template <typename M>
        void send(rank dst, const M& message, int tag);

        template <typename M>
        M recv(int src = MPI_ANY_SOURCE, int tag = MPI_ANY_TAG);

        std::optional<status> available_message(int src = MPI_ANY_SOURCE,
                                                int tag = MPI_ANY_TAG);

        stats_pair_type get_stats() const;

    private:
        msg_stats_map_type send_stats_;
        msg_stats_map_type recv_stats_;
    };

} // namespace mpi

#include "mpi/mpi.hxx"
