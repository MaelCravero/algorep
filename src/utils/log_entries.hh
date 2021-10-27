#pragma once

#include <vector>

#include "utils/logger.hh"

namespace utils
{
    class LogEntries
    {
    public:
        struct Entry
        {
            int term;
            int client_id;
            int data;
            int request_id;
        };

        LogEntries(std::string file);

        void append_entry(int term, int client_id, int data, int request_id);

        int last_log_index() const;
        int last_log_term() const;

        int get_commit_index() const;
        bool commit_next_entry();
        size_t size() const;
        void delete_from_index(int index);

        Entry& operator[](int i);

    private:
        std::vector<Entry> entries_;
        int commit_index_;

        Logger logger_;
    };
} // namespace utils
