#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/riscv.h"
#include "user/user.h"


int
thread_join()
{
  void *stack = 0;
  int pid = join(&stack);
  if(stack) free(stack);
  if(pid < 0) {
    return -1; // Error in join
  }
  return pid; // Return the thread's PID
}
int
thread_create(void(*start_routine)(void*, void*), void *arg1, void *arg2)
{

  void *stack = malloc(PGSIZE << 1); 
  if (stack == 0) {     
    return -1;
  }

  int pid = clone(start_routine, arg1, arg2, stack);
  if (pid < 0) {
    free(stack);
    return -1;
  }

  return pid;
}