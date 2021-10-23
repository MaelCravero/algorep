#include "logger.hh"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>

namespace utils
{
    Logger::Logger(std::string file)
        : stream_(file)
    {}

    void Logger::log(LogType type, std::string message)
    {
        static std::map<LogType, std::string> log2str{{LogType::INFO, "info"},
                                                      {LogType::DEBUG, "dbug"},
                                                      {LogType::WARN, "warn"},
                                                      {LogType::ERROR, "errr"}};

        auto time = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());

        stream_ << "[" << log2str[type] << "] "
                << std::put_time(std::localtime(&time), "[%F %T]") << ": "
                << message << std::endl;
        // Always flushing for now, to get the log even when we interupt the
        // program
    }

    Logger::SubLogger Logger::operator<<(LogType mode)
    {
        return SubLogger(mode, *this);
    }

} // namespace utils
