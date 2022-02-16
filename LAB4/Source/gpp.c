#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]){

	if(argc != 1)
    {
	    printf(1, "This function has no argument\n");
		exit();
	}

    printf(1, "Parent Pid = %d\n" , get_parent_pid());
    exit();  	
} 