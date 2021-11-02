#pragma once

#include <vector>

#include "rpc/rpc.hh"
#include "utils/logger.hh"

namespace utils
{
    class LogEntries
    {
    public:
        struct Entry
        {
            int term;
            rpc::ClientRequest data;
        };

        LogEntries(std::string file);

        void append_entry(int term, rpc::ClientRequest data);

        int last_log_index() const;
        int last_log_term() const;

        int get_commit_index() const;
        bool commit_next_entry();
        size_t size() const;
        void delete_from_index(unsigned index);

        Entry& operator[](int i);

    private:
        std::vector<Entry> entries_;
        int commit_index_;

        Logger logger_;
    };
} // namespace utils
