#include "utils/log_entries.hh"

#define LOG(mode) logger_ << Logger::LogType::mode

namespace utils
{
    LogEntries::LogEntries(std::string file)
        : entries_()
        , commit_index_(-1)
        , logger_(file)
    {}

    void LogEntries::append_entry(int term, int client_id, int data,
                                  int request_id)
    {
        for (const auto& entry : entries_)
        {
            // if entry already in the log do not add it again
            if (entry.term == term && entry.client_id == client_id
                && entry.data == data && entry.request_id == request_id)
                return;
        }

        entries_.emplace_back(Entry{term, client_id, data, request_id});
    }

    int LogEntries::last_log_index() const
    {
        int size = entries_.size();

        if (size)
            return size - 1;
        return -1;
    }

    int LogEntries::last_log_term() const
    {
        if (entries_.size())
            return entries_.back().term;
        return -1;
    }

    int LogEntries::get_commit_index() const
    {
        return commit_index_;
    }

    bool LogEntries::commit_next_entry()
    {
        if (commit_index_ >= last_log_index())
            return false;

        commit_index_++;

        const auto entry = entries_[commit_index_];
        LOG(INFO) << "term: " << entry.term << " client_id: " << entry.client_id
                  << " data: " << entry.data;

        return true;
    }

    size_t LogEntries::size() const
    {
        return entries_.size();
    }

    void LogEntries::delete_from_index(int index)
    {
        if (index < entries_.size())
            entries_.resize(index);
    }

    LogEntries::Entry& LogEntries::operator[](int i)
    {
        return entries_[i];
    }
} // namespace utils
