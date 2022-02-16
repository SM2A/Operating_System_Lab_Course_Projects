#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
	    printf(1, "Please enter only one argument\n");
        exit();
	}
    int traced_pid = atoi(argv[1]);
    int prev_value = 0;
    asm volatile(
        "movl %%ebx, %0;"
        "movl %1, %%ebx;"
        : "=r" (prev_value)
        : "r"(traced_pid)
    );

	set_process_parent();

    asm("movl %0, %%ebx" : : "r"(prev_value));
    wait();

    printf(2, "Tracer process closed\n");
    
    exit();  
}