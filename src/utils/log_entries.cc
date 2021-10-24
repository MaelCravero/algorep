#include "utils/log_entries.hh"

#define LOG(mode) logger_ << Logger::LogType::mode

namespace utils
{
    LogEntries::LogEntries(std::string file)
        : entries_()
        , commit_index_(-1)
        , logger_(file)
    {}

    void LogEntries::append_entry(int term, int client_id, int data)
    {
        entries_.emplace_back(Entry{term, client_id, data});
    }

    std::optional<int> LogEntries::last_log_index() const
    {
        int size = entries_.size();

        if (size)
            return size - 1;
        return {};
    }

    std::optional<int> LogEntries::last_log_term() const
    {
        if (entries_.size())
            return entries_.back().term;
        return {};
    }

    int LogEntries::get_commit_index() const
    {
        return commit_index_;
    }

    void LogEntries::commit_next_entry()
    {
        commit_index_++;

        const auto entry = entries_[commit_index_];
        LOG(INFO) << "term: " << entry.term << " client_id: " << entry.client_id
                  << " data: " << entry.data;
    }

    LogEntries::Entry& LogEntries::operator[](int i)
    {
        return entries_[i];
    }
} // namespace utils
