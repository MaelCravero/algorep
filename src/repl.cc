#include "repl.hh"

#include <string>

#include "mpi/mpi.hh"

Repl::Repl(int nb_server)
    : nb_server_(nb_server)
{}

void Repl::operator()()
{
    std::cout << "repl\n";
    std::string line;

    while (std::cin)
    {
        std::getline(std::cin, line);
        std::cout << line << "\n";
    }

    std::cout << "repl done\n";
}
