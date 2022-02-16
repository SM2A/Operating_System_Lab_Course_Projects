#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf(1, "please enter the HRRN priority\n");
        exit();
    }
    set_ptable_hrrn_priority(atoi(argv[1]));
    
    exit();
}