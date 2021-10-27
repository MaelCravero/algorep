#pragma once
#include <chrono>
#include <random>

namespace utils
{
    using timestamp = std::chrono::duration<double>;

    inline timestamp now()
    {
        return std::chrono::system_clock::now().time_since_epoch();
    }

    /// get a new timeout between `min` and `max` seconds from now
    inline timestamp get_new_timeout(double min, double max)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dis(min, max);

        int delay = dis(gen) * 1000;
        return now() + std::chrono::milliseconds(delay);
    }

} // namespace utils
