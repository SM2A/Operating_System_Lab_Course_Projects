#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    if(argc != 3)
    {
        printf(1, "Please enter the priority and the process id.\n");
        exit();
    }

    set_hrrn_priority(atoi(argv[1]), atoi(argv[2]));

    exit();
}