#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]){

	if(argc != 2)
    {
	    printf(1, "Please enter only one argument\n");
		exit();
	}


    int num = atoi(argv[1]);
    int prev_value = 0;

    asm volatile(
        "movl %%ebx, %0;"
        "movl %1, %%ebx;"
        : "=r" (prev_value)
        : "r"(num)
    );

    printf(1, "Sum = %d\n" , calculate_sum_of_digits());

    asm("movl %0, %%ebx" : : "r"(prev_value));
    
    exit();  	
} 