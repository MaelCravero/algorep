#pragma once
#include <chrono>
#include <random>
#include <thread>

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

    inline void sleep_for_ms(unsigned ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    class Timeout
    {
    public:
        Timeout(double lower_bound, double upper_bound)
            : lower_bound(lower_bound)
            , upper_bound(upper_bound)
            , speed_mod(1)
            , timeout_()
        {
            reset();
        }

        inline void reset()
        {
            timeout_ = utils::get_new_timeout(lower_bound * speed_mod,
                                              upper_bound * speed_mod);
        }
        inline operator bool()
        {
            return now() > timeout_;
        }

        double lower_bound;
        double upper_bound;
        int speed_mod;

    private:
        timestamp timeout_;
    };

} // namespace utils
