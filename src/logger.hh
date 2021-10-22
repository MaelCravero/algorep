#pragma once

#include <fstream>

class Logger
{
public:
    enum class LogType
    {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
    };

    Logger(std::string file);

    void log(LogType type, std::string message);

private:
    std::ofstream stream_;
};
