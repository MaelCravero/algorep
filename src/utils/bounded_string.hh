#include <array>
#include <cstring>
#include <string>

#pragma once

namespace utils
{
    // template <std::size_t N>
    // struct bounded_string : public std::array<char, N>
    // {
    //     using super_type = std::array<char, N>;

    //     bounded_string() = default;

    //     bounded_string(std::string s)
    //         : super_type()
    //     {
    //         std::memcpy(this->data(), s.data(), N);
    //         (*this)[N - 1] = 0;
    //     }

    //     bounded_string(const bounded_string& s)
    //         : super_type()
    //     {
    //         std::memcpy(this->data(), s.data(), N);
    //         (*this)[N - 1] = 0;
    //     }

    //     inline bounded_string& operator=(std::string s)
    //     {
    //         std::memcpy(this->data(), s.data(), N);
    //         (*this)[N - 1] = 0;
    //         return *this;
    //     }

    //     inline operator std::string()
    //     {
    //         return std::string(this->data());
    //     }
    // };

    template <std::size_t N>
    class bounded_string
    {
    public:
        bounded_string() = default;

        bounded_string(std::string s)
        {
            auto len = std::min(N - 1, s.size());
            std::memcpy(data_, s.data(), len);
            (*this)[len] = 0;
        }

        bounded_string(const bounded_string& s)
        {
            std::memcpy(data_, s.data_, N);
        }

        inline bounded_string& operator=(std::string s)
        {
            auto len = std::min(N - 1, s.size());
            std::memcpy(data_, s.data(), len);
            (*this)[len] = 0;
            return *this;
        }

        inline operator std::string()
        {
            return std::string(data_);
        }

        inline char& operator[](std::size_t i)
        {
            return data_[i];
        }

        inline bool operator==(const bounded_string& other)
        {
            return !memcmp(data_, other.data_, N);
        }

    private:
        char data_[N];
    };

    template <size_t N>
    inline std::ostream& operator<<(std::ostream& ostr, bounded_string<N> s)
    {
        std::string string = s;
        return ostr << string;
    }

} // namespace utils
