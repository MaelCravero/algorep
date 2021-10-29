#include "repl.hh"

#include <iostream>
#include <sstream>

#include "mpi/mpi.hh"

Repl::Repl(int nb_server, int nb_client)
    : nb_server_(nb_server)
    , nb_client_(nb_client)
{}

std::optional<Repl::Command> Repl::parse_command(std::string line)
{
    auto iss = std::istringstream(line);
    std::string str;

    iss >> str;

    Command command;

    if (str == "SPEED")
    {
        command.order = Order::SPEED;
        iss >> str;

        if (str == "low")
            command.speed_level = 10;
        else if (str == "medium")
            command.speed_level = 3;
        else if (str == "high")
            command.speed_level = 1;
        else
            return {};
    }

    else if (str == "CRASH")
        command.order = Order::CRASH;
    else if (str == "START")
        command.order = Order::BEGIN;
    else if (str == "RECOVERY")
        command.order = Order::RECOVERY;
    else if (str == "STATUS")
        command.order = Order::PRINT;
    else
        return {};

    try
    {
        if (!iss.eof())
        {
            iss >> str;
            command.target = std::stoi(str);
        }
        else
            command.target = 0;
    }
    catch (const std::invalid_argument&)
    {
        return {};
    }

    return command;
}

void Repl::execute(Command command)
{
    std::cout << "sending message " << static_cast<char>(command.order) << " "
              << command.target << " " << command.speed_level << "\n";

    ReplMessage message;
    message.order = command.order;
    message.speed_level = command.speed_level;

    // TODO: check if target is valid

    if (command.target)
    {
        if (command.order == Order::BEGIN && command.target <= nb_client_)
            command.target += nb_server_;

        mpi::send(command.target, message, MessageTag::REPL);
    }

    else
    {
        if (command.order != Order::BEGIN)
            for (int i = 1; i <= nb_server_; i++)
                mpi::send(i, message, MessageTag::REPL);
        else
            for (int i = nb_server_ + 1; i <= nb_server_ + nb_client_; i++)
                mpi::send(i, message, MessageTag::REPL);
    }
}

void Repl::operator()()
{
    std::cout << "repl\n";
    std::string line;

    while (std::cin)
    {
        std::getline(std::cin, line);
        auto command = parse_command(line);

        if (command)
            execute(command.value());
    }

    std::cout << "repl done\n";
}
