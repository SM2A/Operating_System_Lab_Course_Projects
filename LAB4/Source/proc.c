
#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->q = 2;
  p->creation_time = ticks;
  p->waiting_time = 0;
  p->executed_cycle_number = 1;
  p->hrrn_priority = 0;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

int get_mhrrn(struct proc* process)
{
    int waiting_time = ticks - process->creation_time;
    float hrrn = (waiting_time - process->executed_cycle_number) * 1.0 / process->executed_cycle_number;
    float mhrrn = (hrrn + process->hrrn_priority) / 2;
    return mhrrn;
}

struct proc* rr_next(void){
  struct proc* process;
  struct proc* next = 0;

  int now = ticks;
  int process_max = -99999999;

  for (process = ptable.proc ; process < &ptable.proc[NPROC] ; process++){
    if ((process->state != RUNNABLE) || (process->q != 1)) continue;
    if (process_max < (now - process->last_processor_time)){
      process_max = now - process->last_processor_time;
      next = process;
    }
  }

  return next;
}

struct proc* lcfc(void){
  struct proc* process;
  struct proc* next = 0;

  int last_creation_time = -1;

  for (process = ptable.proc ; process < &ptable.proc[NPROC] ; process++){
    if ((process->state != RUNNABLE) || (process->q != 2)) continue;
    if (last_creation_time < process->creation_time){
      last_creation_time = process->creation_time;
      next = process;
    }
  }
  return next;
}

struct proc* mhrrn(void){
  struct proc* process;
  struct proc* next = 0;

  int max_mhrrn = -1;
  

  for (process = ptable.proc ; process < &ptable.proc[NPROC] ; process++){
    if ((process->state != RUNNABLE) || (process->q != 3)) continue;

    float mhrrn = get_mhrrn(process);
    
    if (max_mhrrn < mhrrn){
      max_mhrrn = mhrrn;
      next = process;
    }
  }

  return next;
}

void update_waiting_time(void) {
  struct proc* process;
  for (process = ptable.proc ; process < &ptable.proc[NPROC] ; process++){
    if (process->q == 1) continue;
    if (process->waiting_time > 8000 && process->state == RUNNABLE) {
      process->waiting_time = 0;
      process->q = 1;
      continue;
    }
    if (process->state == RUNNABLE)
      process->waiting_time += 1;
  }
}



//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    acquire(&ptable.lock);

    update_waiting_time();
        
    p = rr_next();

    if (p == 0) 
    {
      p = lcfc();
      if (p == 0) 
        p = mhrrn();
    }

    if (p == 0){
      release(&ptable.lock);
      continue;
    }

    p->waiting_time = 0;
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    swtch(&(c->scheduler), p->context);
    switchkvm();
    c->proc = 0;
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  myproc()->last_processor_time = ticks;
  myproc()->executed_cycle_number += 1;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int calculate_sum_of_digits(int n)
{
  int result = 0;

    while (n != 0)
    {
        result += (n % 10);
        n /= 10;
    }

    return result;
}

void set_process_parent(int pid)
{
  struct proc* p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->pid == pid)
    {
      break;
    }
  }
  struct proc* myp = myproc();
  myp->is_tracer = 1;
  myp->tracer_parent = p->parent;
  myp->traced_process = p;
  p->parent = myproc();
  cprintf("process %d parent changed to %d\n", p->pid, myp->pid);
}

void change_process_queue(int pid, int dest_q)
{
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      break;
    }
  }
  p->q = dest_q;
  p->waiting_time = 0;
  cprintf("process %d priority changed to %d\n", p->pid, dest_q);

  release(&ptable.lock);
}

void set_hrrn_priority(int pid, int new_priority)
{
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      break;
    }
  }
  p->hrrn_priority = new_priority;
  cprintf("process %d HRRN priority changed to %d\n", p->pid, new_priority);

  release(&ptable.lock);
}

void set_ptable_hrrn_priority(int new_priority)
{
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    p->hrrn_priority = new_priority;
  }
  cprintf("all processes HRRN priority changed to %d\n", new_priority);

  release(&ptable.lock);
}

char* get_proc_state(int state)
{
  if(state == UNUSED)
    return "UNUSED";
  else if(state == EMBRYO)
    return "EMBRYO";
  else if(state == SLEEPING)
    return "SLEEPING";
  else if(state == RUNNABLE)
    return "RUNNABLE";
  else if(state == RUNNING)
    return "RUNNING";
  else if(state == ZOMBIE)
    return "ZOMBIE";
  else
    return "UNKNOWN";
}

int get_lenght(int num)
{
  int len = 0;
  if(num == 0)
    return 1;
  while(num > 0)
  {
    num /= 10;
    len++;
  }
  return len;
}

void print_processes(void)
{
  struct proc *p;
  acquire(&ptable.lock);
  cprintf("name      pid       state       queue_level       cycle        arrival     HRRN     MHRRN\n");
  cprintf(".........................................................................................\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->state == UNUSED)
      continue;
    cprintf("%s", p->name);
    for(int i = 0; i < 10 - strlen(p->name); i++) cprintf(" ");

    cprintf("%d", p->pid);
    for(int i = 0; i < 8 - get_lenght(p->pid); i++) cprintf(" ");

    cprintf("%s", get_proc_state(p->state));
    for(int i = 0; i < 17 - strlen(get_proc_state(p->state)); i++) cprintf(" ");

    cprintf("%d", p->q);
    for(int i = 0; i < 17 - get_lenght(p->q); i++) cprintf(" ");

    cprintf("%d", p->executed_cycle_number);
    for(int i = 0; i < 15 - get_lenght(p->executed_cycle_number); i++) cprintf(" ");

    cprintf("%d", p->creation_time);
    for(int i = 0; i < 10 - get_lenght(p->creation_time); i++) cprintf(" ");

    cprintf("%d", p->hrrn_priority);
    for(int i = 0; i < 10 - get_lenght(p->hrrn_priority); i++) cprintf(" ");

    cprintf("%d", get_mhrrn(p));
    for(int i = 0; i < 5; i++) cprintf(" ");
    cprintf("\n");

  }
  release(&ptable.lock);
}

struct semaphore {
  int value;
  int locked;
  int owner;
  struct spinlock lock;
};

struct semaphore chop_stick[6];

int sem_init(int i, int v){

  acquire(&chop_stick[i].lock);

  if (chop_stick[i].locked == 0) {
    chop_stick[i].locked = 1;
    chop_stick[i].value = v;
    chop_stick[i].owner = -1;
  } else {
    release(&chop_stick[i].lock);
    return -1;
  }  

  release(&chop_stick[i].lock);

  return 0;

}

int sem_acquire(int i){

  acquire(&chop_stick[i].lock);

  if (chop_stick[i].value >= 1) {
     chop_stick[i].value = chop_stick[i].value - 1;
     chop_stick[i].owner = i;
  } else {
    while (chop_stick[i].value < 1) sleep(&chop_stick[i],&chop_stick[i].lock);
    chop_stick[i].value = chop_stick[i].value - 1;
    chop_stick[i].owner = i;
  }

  release(&chop_stick[i].lock);

  return 0;
}

int sem_release(int i){
  
  acquire(&chop_stick[i].lock);
  chop_stick[i].value = chop_stick[i].value + 1;
  chop_stick[i].owner = -1;
  wakeup(&chop_stick[i]); 
  release(&chop_stick[i].lock);

  return 0;
}
