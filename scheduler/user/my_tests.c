#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int make_process(int n){
  /*
    make n processes. The parent process is i=n.
  */
  int i;
  for(i = 0; i < n; i++)
  {
    int p = fork();
    if(p == 0) break;
  }
  return i;
}

void 
test1(int n)
{
  // Does fcfs works well?
  /*
    make process 5 and check the print of i is 0 -> 1 -> 2 -> ... -> n - 1
  */
  int i = make_process(n);
   
  if(i == n)
  {
    int x;
    while(wait(&x) > 0);
    return;
  }
  printf("%d\n", i);
  for(int k = 0; k < 10000; k++) if(k % 99999 == 0) printf("%d\n", i);
  printf("%d\n", i);
  exit(0);
}
void 
test2(int n)
{
  // Does mlfq works well?
  /*
    when level 0 and 1 check the print of i is 0 -> 1 -> 2 -> ... -> n - 1
    and level 2 check the print of i is 0 -> 1 ->2 ... -> n - 1 -> 0 -> 1 ->2 ... -> n - 1 -> any order;
    print form: [i: level of i]
    this test runs with turning off priority boosting.
  */
  int i = make_process(n);
  if(i == n)
  {
    int xstatus;
    mlfqmode();
    while(wait(&xstatus) > 0);
    return;
  }
  int preb = -1;
  for(int k = 0; k < 100000; k++)
  {
    int level = getlev();
    if(k % 50000 == 0 || level != preb)
    {
      preb = level;
      printf("%d: %d\n", i, level);
    }
  }
  printf("fin %d\n", i);
  exit(0);
}

void 
test3(int n)
{
  // Does mode change works well?
  /*
    when level 0 and 1 check the print of i is 0 -> 1 -> 2 -> ... -> n - 1
    and level 2 check the print of i is 0 -> 1 ->2 ... -> n - 1 -> 0 -> 1 ->2 ... -> n - 1 -> any order;
  */
  int i = make_process(n);
  if(i == n)
  {
    int xstatus;
    mlfqmode();
    while(wait(&xstatus) > 0);
    return;
  }
  int r = 0;
  for(int k = 0; k < 210100; k++)
  {
    r = getlev();
    if(k % 100000 == 0) r = mlfqmode();
    else if(k % 50000 == 0 ) r = fcfsmode();
    if(k % 10000 == 0) printf("%d: %d\n", i, k);
    if(k) r = 1;
  }
  if(r);
  printf("fin %d\n", i);
  exit(0);
}
void 
test4(int n)
{
  // Does setpriority and L2 queue work well?
  /*
    always make process i priority set i (0<=i<=3)
    also if level != 2 then busy waiting
    So if setpriority works well, we can expect that mlfq works like almost fcfs.
  */
  int i = make_process(n);
  if(i == n) {
    int xstatus;
    mlfqmode();
    while(wait(&xstatus) > 0);
    return;
  }
  int pid = getpid();
  for(int k = 0; k < 12100; k++)
  {
    setpriority(pid, i);
    while(getlev() != 2) ;
    setpriority(pid, i);
    if(k % 3000 == 0) {
      while(getlev() != 2);
      setpriority(pid, i);
      printf("%d\n", i);
    }
  }
  printf("fin %d\n", i);
  exit(0);
}
void
test5(int n)
{
  // Does yield work well?
  /*
    produce P1 and P2. And with yield takes turn printing 1 and 2.
    Check yield does not decrease tick of a process -> check level change.
    below is expected results.
    0: 1
    1: 1
    0: 1
    1: 1
    0: 1
    1: 1
    0: 2
    1: 2
  */
  fcfsmode(); 
  int i = make_process(n);
  if(i == n){
    int x;
    printf("i: level\n");;
    mlfqmode(); 
    while(wait(&x) > 0);
    return;
  }
    for(int j = 0; j < 5; j++){
    printf("%d: %d\n", i, getlev());
    yield();
  }

  exit(0);
}

void main()
{
  printf("----------------test1 start----------------\n");
  test1(5);
  printf("----------------test2 start----------------\n");
  test2(5);
  printf("----------------test3 start----------------\n");
  test3(3);
  printf("----------------test4 start----------------\n");
  test4(4);
  printf("----------------test5 start----------------\n");
  test5(2);
}
