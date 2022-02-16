#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
    if(argc != 1)
    {
	    printf(1, "Too many arguments!!!\n");
		exit();
	}

    printf(1, "Traced pid is %d\n" , getpid());
	printf(1, "Traced parent pid is %d\n", get_parent_pid());
    sleep(1000);

	printf(1, "Traced parent pid is %d\n", get_parent_pid());
    exit();  
}