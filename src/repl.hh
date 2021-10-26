#pragma once

#include <optional>
#include <string>

#include "common.hh"

class Repl
{
public:
    enum class Order : char
    {
        SPEED = 's',
        CRASH = 'c',
        BEGIN = 'b',
        RECOVERY = 'r',
        PRINT = 'p',
    };

    struct ReplMessage : public Message
    {
        Order order;
        int speed_level; // optional argument for some orders
    };

private:
    struct Command
    {
        Order order;
        int target;
        int speed_level;
    };

public:
    Repl(int nb_server, int nb_client);

    std::optional<Command> parse_command(std::string line);
    void execute(Command command);
    void operator()();

private:
    int nb_server_;
    int nb_client_;
};
