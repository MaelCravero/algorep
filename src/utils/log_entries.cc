#include "utils/log_entries.hh"

#include <iostream>

#define LOG(mode) logger_ << Logger::LogType::mode

namespace utils
{
    LogEntries::LogEntries(std::string file)
        : entries_()
        , commit_index_(-1)
        , logger_(file)
    {}

    void LogEntries::append_entry(int term, rpc::ClientRequest data)
    {
        for (const auto& entry : entries_)
        {
            // if entry already in the log do not add it again
            if (entry.data == data)
                return;
        }

        entries_.emplace_back(Entry{term, data});
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
        LOG(INFO) << "term: " << entry.term << ", client: " << entry.data.source
                  << ", command: " << entry.data.command << ", id "
                  << entry.data.id;

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
