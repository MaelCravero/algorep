#pragma once

#include <optional>
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
        };

        LogEntries(std::string file);

        void append_entry(int term, int client_id, int data);

        std::optional<int> last_log_index() const;
        std::optional<int> last_log_term() const;

        int get_commit_index() const;
        void commit_next_entry();

        Entry& operator[](int i);

    private:
        std::vector<Entry> entries_;
        int commit_index_;

        Logger logger_;
    };
} // namespace utils
