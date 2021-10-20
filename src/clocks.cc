#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

#define LEN 10
char buf[LEN] = "PING";
MPI_Status status;

//#define PING(dst) MPI_Ssend(buf, LEN, MPI_CHAR, dst, 0, MPI_COMM_WORLD);
//#define WAIT(src) MPI_Recv(buf, LEN, MPI_CHAR, src, 0, MPI_COMM_WORLD,
//&status);

#define PING(dst) ping_time(dst, rank)
#define WAIT(dst) wait_time(dst, rank)

struct Lamport
{
    int time;
};

struct Lamport make_lamport()
{
    struct Lamport res = {0};
    return res;
}

void update_lamport(struct Lamport* clock, int time)
{
    if (time > clock->time)
        clock->time = time;
}

void inc_lamport(struct Lamport* clock)
{
    clock->time++;
}

struct Mattern
{
    int* time;
    int size;
};

struct Mattern make_mattern(int size)
{
    int* time = (int*)calloc(sizeof(int), size);

    assert(time != NULL);

    struct Mattern res = {time, size};
    return res;
}

void free_mattern(struct Mattern* clock)
{
    if (clock)
        free(clock->time);
}

void update_mattern(struct Mattern* clock, int* time)
{
    for (int i = 0; i < clock->size; i++)
        if (time[i] > clock->time[i])
            clock->time[i] = time[i];
}

void inc_mattern(struct Mattern* clock, int rank)
{
    assert(rank < clock->size);

    clock->time[rank]++;
}

static struct Lamport lamport;
static struct Mattern mattern;

void print_time(char prefix[], int rank)
{
    assert(rank < mattern.size);
    printf("%s: %d has time L%d, M(%d", prefix, rank, lamport.time,
           mattern.time[0]);

    for (int i = 1; i < mattern.size; i++)
        printf(", %d", mattern.time[i]);

    printf(")\n");
}

void ping_time(int dst, int rank)
{
    inc_lamport(&lamport);
    inc_mattern(&mattern, rank);

    int* time = (int*)calloc(sizeof(int), mattern.size + 1);

    time[0] = lamport.time;
    memcpy(time + 1, mattern.time, mattern.size * sizeof(int));

    print_time("SEND", rank);
    MPI_Ssend(time, mattern.size + 1, MPI_INT, dst, 0, MPI_COMM_WORLD);

    free(time);
}

void wait_time(int src, int rank)
{
    int* time = (int*)calloc(sizeof(int), mattern.size + 1);
    MPI_Recv(time, mattern.size + 1, MPI_INT, src, 0, MPI_COMM_WORLD, &status);

    update_lamport(&lamport, time[0]);
    update_mattern(&mattern, time + 1);

    print_time("RECV", rank);
}

void ring(int rank, int size)
{
    int src = rank == 0 ? size - 1 : rank - 1;
    int dst = (rank + 1) % size;

    if (rank == 0)
    {
        printf("Rank is %d\n", rank);
        PING(dst);
        WAIT(src);
    }
    else
    {
        WAIT(src);
        printf("Rank is %d\n", rank);
        PING(dst);
    }
}

void central(int rank, int size)
{
    if (rank != 0)
    {
        WAIT(0);
        printf("%d received %s from master\n", rank, buf);
        PING(0);
    }
    else
    {
        int dst = 1;
        while (dst < size)
        {
            printf("Pinging %d\n", dst);
            PING(dst);
            WAIT(dst);
            printf("master received %s from %d\n", buf, dst);
            dst++;
        }
    }
}

void tree(int rank, int size)
{
    int dst = rank * 2 + 1; // left child
    int parent = (rank - 1) / 2;

    if (rank != 0)
    {
        WAIT(parent);
        printf("%d received %s from parent %d\n", rank, buf, parent);
    }

    if (dst < size)
    {
        printf("%d pinging left child %d\n", rank, dst);
        PING(dst);
        WAIT(dst);
        printf("Left child done\n");
        dst++;
        if (dst < size)
        {
            printf("%d pinging right child %d\n", rank, dst);
            PING(dst);
            WAIT(dst);
            printf("Right child done\n");
        }
    }

    if (rank != 0)
    {
        PING(parent);
    }
}

int main(int argc, char* argv[])
{
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    lamport = make_lamport();
    mattern = make_mattern(size);

    /*ring(rank, size);*/
    /*central(rank, size);*/
    tree(rank, size);

    MPI_Finalize();

    return 0;
}
