#include "syscall.h"
#include "traps.h"

#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret

SYSCALL(fork)
SYSCALL(exit)
SYSCALL(wait)
SYSCALL(pipe)
SYSCALL(read)
SYSCALL(write)
SYSCALL(close)
SYSCALL(kill)
SYSCALL(exec)
SYSCALL(open)
SYSCALL(mknod)
SYSCALL(unlink)
SYSCALL(fstat)
SYSCALL(link)
SYSCALL(mkdir)
SYSCALL(chdir)
SYSCALL(dup)
SYSCALL(getpid)
SYSCALL(sbrk)
SYSCALL(sleep)
SYSCALL(uptime)
SYSCALL(calculate_sum_of_digits)
SYSCALL(get_parent_pid)
SYSCALL(get_file_sectors)
SYSCALL(set_process_parent)
SYSCALL(change_process_queue)
SYSCALL(set_hrrn_priority)
SYSCALL(set_ptable_hrrn_priority)
SYSCALL(print_processes)
SYSCALL(sem_init)
SYSCALL(sem_acquire)
SYSCALL(sem_release)