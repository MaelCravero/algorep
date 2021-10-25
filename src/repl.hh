#pragma once

#include "common.hh"

class Repl
{
public:
    enum class Order
    {
        SPEED,
        CRASH,
        START,
        RECOVERY,
        STATUS,
    };

    struct ReplMessage : public Message
    {
        Order order;
        int argument; // optional argument for some orders
    };

    Repl(int nb_server);

    void operator()();

private:
    int nb_server_;
};
