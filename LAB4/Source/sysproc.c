#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int sys_calculate_sum_of_digits(void)
{
  int num = myproc()->tf->ebx;
  cprintf("sys_calculate_sum_of_digits : %d\n",num);
  return calculate_sum_of_digits(num);
}

int sys_get_parent_pid(void)
{
  struct proc *p = myproc()->parent;
  while (p->is_tracer) {
    p = p->tracer_parent;
  }
  return p->pid;
}

void sys_set_process_parent(void)
{
  int pid = myproc()->tf->ebx;
  cprintf("sys_set_process_parent for process %d\n", pid);
  return set_process_parent(pid);
}

int sys_change_process_queue(void)
{
  int pid ,new_priority;
  argint(0, &pid);
  argint(1, &new_priority);
  change_process_queue(pid, new_priority);
  return 0;
}

int sys_set_hrrn_priority(void)
{
  int pid ,new_priority;
  argint(0, &pid);
  argint(1, &new_priority);
  set_hrrn_priority(pid, new_priority);
  return 0;
}

int sys_set_ptable_hrrn_priority(void)
{
  int new_priority;
  argint(0, &new_priority);
  set_ptable_hrrn_priority(new_priority);
  return 0;
}

int sys_print_processes(void)
{
  print_processes();
  return 0;
}

int sys_sem_init(void){
  int i , j;
  argint(0,&i);
  argint(1,&j);
  return sem_init(i,j);
}

int sys_sem_acquire(void){
  int i;
  argint(0,&i);
  return sem_acquire(i);
}

int sys_sem_release(void){
  int i;
  argint(0,&i);
  return sem_release(i);
}
