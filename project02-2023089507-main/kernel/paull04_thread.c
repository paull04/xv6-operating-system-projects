#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sleeplock.h"


extern struct proc proc[NPROC];
extern struct spinlock wait_lock;
extern void forkret(void);
extern void usertrapret(void);
extern uint64 allocpid(void);

static struct proc*
allocthread(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;
  p->killed = 0;
  p->xstate = 0;
  

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    p->state = UNUSED;
    release(&p->lock);
    return 0;
  }
  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;
  return p;
}

int
paull04_clone(uint64 fcn, uint64 arg1, uint64 arg2, uint64 stack)
{
  struct proc *p = myproc();
  struct proc *np;
  uint64 sp;

  // Allocate process.
  if((np = allocthread()) == 0){
    return -1;
  }
  p->main_thread->cnt++; // Increment the thread count of the main thread
  // Share user memory from parent to child.
  np->pagetable = p->pagetable;
  np->sz = p->sz;
  //*(np->trapframe) = *(p->trapframe);
  sp = PGROUNDUP(stack); // Align stack to page size
  np->stack_base = stack; // Store the base of the stack for this thread
  // Cause fork to return 0 in the child.
  *(np->trapframe) = *(p->trapframe); // Copy the parent's trapframe
  np->trapframe->sp = sp + PGSIZE; // Set the stack pointer to the top of the new stack
  np->trapframe->ra = 0xFFFFFFFFFFFFFFFF; // Set return address to an invalid value
  np->trapframe->a0 = arg1;
  np->trapframe->a1 = arg2;
  np->trapframe->epc = fcn; // Set the entry point to the function to be executed
  np->trapframe->kernel_sp = np->kstack + PGSIZE; // Set the kernel stack pointer
    //printf("paull04_clone: mappages succeeded for trapframe at %lu\n", np->trapframe_va);

  np->trapframe_va = TRAPFRAME - (p->main_thread->cnt * PGSIZE); // Set the virtual address for the trapframe
  if(mappages(p->pagetable, np->trapframe_va, PGSIZE,
              (uint64)(np->trapframe), PTE_R | PTE_W) < 0){
    panic("paull04_clone: mappages failed");
    return 0;
  }
  //printf("paull04_clone: mappages succeeded for trapframe at %lu\n", np->trapframe_va);
  // increment reference counts on open file descriptors.
  for(int i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  
  np->cwd = idup(p->cwd);
  
  safestrcpy(np->name, p->name, sizeof(p->name));
  
  np->state = RUNNABLE;
  np->main_pid = p->main_pid; // Set the main thread's PID
  np->main_thread = p->main_thread; // Set the main thread pointer
  np->isThread = 1; // Mark this as a thread
  np->parent = p->main_thread;
  int pid = np->pid;
  release(&np->lock);
  return pid;
}
void freethread(struct proc *p) {
  //printf("freethread: freeing thread with PID %d\n", p->pid);
  reparent(p);
  if(p->isThread) uvmunmap(p->pagetable, p->trapframe_va, 1, 0); 
  p->trapframe_va = 0; // Clear the trapframe virtual address
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;

  kfree((void *)p->trapframe);
  p->trapframe = 0;
  p->trapframe_va = 0;
  p->main_pid = 0; // Clear the main PID
  p->main_thread = 0; // Clear the main thread pointer
  p->isThread = 0; // Mark this as not a thread anymore
}
void cleanAllThreads(struct proc *p) {
  struct proc *np;
  acquire(&wait_lock);
  for(np = proc; np < &proc[NPROC]; np++) {
    if(np->main_pid == p->pid && np->isThread) {
      acquire(&np->lock);

      freethread(np); // Free the thread
      release(&np->lock);
    }
  }
  release(&wait_lock);
}

int
paull04_join(uint64 stack){
  struct proc *p = myproc();
  struct proc *np;
  int havekids, pid;
  
  acquire(&wait_lock);
   
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      if(np->main_pid == p->main_pid && np->isThread){
        acquire(&np->lock);
        
        havekids = 1;
        if(np->state == ZOMBIE){
          if(copyout(p->pagetable, stack, (char *)&np->stack_base, sizeof(uint64)) < 0){
            release(&np->lock);
            release(&wait_lock);
          }
          //printf("paull04_join: found zombie thread with PID %d\n", np->pid);
          pid = np->pid;
          freethread(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }
    if(!havekids || p->killed){
      release(&wait_lock);
      // printf("paull04_join: no threads to join or process killed\n");
      return -1;
    }
    sleep(p, &wait_lock); //DOC: wait-sleep
  }
}

uint64
sys_clone(void)
{
  uint64 fcn, arg1, arg2, stack;
  
  argaddr(0, &fcn);
  argaddr(1, &arg1);
  argaddr(2, &arg2);
  argaddr(3, &stack);

  if(fcn < 0 || arg1  < 0 || arg2 < 0 || stack < 0) return -1;
  
  return paull04_clone(fcn, arg1, arg2, stack);
}
uint64
sys_join(void)
{
  uint64 stack;
  argaddr(0, &stack);

  // printf(" >> %lu\n", stack);
  
  return paull04_join((uint64)stack);
}
