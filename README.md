# AlgoRep

C++20 implementation of the RAFT algorithm for the *ALGOrithmique REPartie*
class.

The REPL can be run using ``make`` or ``make run``.

Several variables can be changed to modify the number of client, server and
number of command per client

Modify NSERVER to change the number of server
Modify NCLIENT to change the number of client
Modify NCMD to change the number of command

Also, CMD_FILE can be modified to use other commands for clients.

for instance you can run the system with 10 servers, 15 clients with 5 commands
each with

``make run NSERVER=10 NCLIENT=15 NCMD=5``

It accepts the following
commands:

- ``STATUS [id]`` gather information on server with rank ``id`` or all servers
  if no argument is given.

- ``START [id]`` start client with rank ``id`` or all clients if no argument is
  given.

- ``CRASH [id]`` crash server with rank ``id`` or all servers if no argument is
  given.

- ``RECOVERY [id]`` restart server with rank ``id`` or all servers if no
  argument is given.

- ``SPEED {low, medium, high} [id]`` set speed of server with rank ``id`` or all
  servers if no ``id`` is given.

- ``WAIT`` wait 3 seconds before asking another REPL command.

- ``STOP [id]`` stop process with rank ``id`` or all processes if no argument is
  given.

Clients will stop on their own if they sent all their requests.
You can stop the REPL process with an EOF.
