#pragma once

#include <fstream>
#include <sstream>

namespace utils
{
    class Logger
    {
    public:
        enum class LogType
        {
            DEBUG,
            INFO,
            WARN,
            ERROR,
        };

        Logger(std::string file);

        void log(LogType type, std::string message);

        class SubLogger
        {
        private:
            SubLogger(LogType mode, Logger& logger)
                : mode_(mode)
                , logger_(logger)
                , message_()
            {}

        public:
            ~SubLogger()
            {
                logger_.log(mode_, message_.str());
            }

            template <typename T>
            inline SubLogger& operator<<(const T& message)
            {
                message_ << message;
                return *this;
            }

        private:
            LogType mode_;
            Logger& logger_;
            std::stringstream message_;

            friend class Logger;
        };

        SubLogger operator<<(LogType mode);

    private:
        std::ofstream stream_;
    };

} // namespace utils
