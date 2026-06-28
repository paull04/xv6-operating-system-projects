#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"


extern struct proc proc[NPROC];
struct queue mlfq[3], fcfs;
struct spinlock schedule_lock;
int schedule_mode = FCFS_MODE;
int priority_boost = 0;
struct spinlock priority_boost_lock;

void
init_my_mlfq()
{
  for(int i = 0; i < 3; i++) acquire(&mlfq[i].lock);
  for(int i = 0; i < 3; i++) mlfq[i].s = mlfq[i].e = 0;
  for(int i = 0; i < 3; i++) release(&mlfq[i].lock);
}
void 
init_my_fcfs()
{
  acquire(&fcfs.lock);
  fcfs.s = fcfs.e = 0;
  release(&fcfs.lock);
}


void 
init_my_lock(){
  initlock(&mlfq[0].lock, "L0 lock");
  initlock(&mlfq[1].lock, "L1 lock");
  initlock(&mlfq[2].lock, "L2 lock");
  initlock(&fcfs.lock, "fcfs lock");
  initlock(&schedule_lock, "schedule mode lock");
  initlock(&priority_boost_lock, "priority boost lock");
  init_my_fcfs();
  init_my_mlfq();
}

void 
queue_push(struct queue *q, struct proc *p)
{
  q->p[q->e] = p;
  q->e = (q->e + 1) & QMOD;
}
static void 
my_swap(struct proc **p, struct proc **q)
{
  struct proc *tmp = *p;
  *p = *q;
  *q = tmp;
}
struct 
proc *queue_pop(struct queue *q)
{
  struct proc *p = q->p[q->s];
  q->s = (q->s + 1) & QMOD;
  return p;
}
int 
isEmpty(struct queue *q)
{
  int k;
  acquire(&q->lock);
  k = q->s == q->e;
  release(&q->lock);
  return k;
}

struct proc *
L2_pop(struct queue *q)
{
  struct proc *r = 0;
  int pr = -1;
  int idx = q->e;
  for(int i = 0; i < q->e; i++) {
    acquire(&q->p[i]->lock);
    if(pr < q->p[i]->priority) {
      r = q->p[i];
      pr = r->priority;
      idx = i;
    }
    release(&q->p[i]->lock);
  }
  q->e--;
  for(int i = idx; i < q->e; i++) my_swap(&q->p[i], &q->p[i+1]);
  return r;
}
void 
L2_push(struct queue *q, struct proc *p)
{
  q->p[q->e++] = p;
}
static struct proc *
pop(struct queue *q)
{
  struct proc *p;
  acquire(&q->lock);
  if (q == mlfq + 2) p = L2_pop(q);
  else p = queue_pop(q);
  release(&q->lock);
  return p;
}
static void 
push(struct queue *q, struct proc *p)
{
  acquire(&q->lock);
  if (q == mlfq + 2)
    L2_push(q, p);
  else
    queue_push(q, p);
  release(&q->lock);
}

void 
my_queueing(struct proc *p)
{
  acquire(&schedule_lock);
  if (schedule_mode == FCFS_MODE) push(&fcfs, p);
  else
  {
    if (--p->time_quantum == 0)
        {
      if (p->level != 2)
        p->level++;
      else if (p->priority > 0)
        p->priority--;
      p->time_quantum = p->level * 2 + 1;
    }
    push(mlfq + p->level, p);
  }
  release(&schedule_lock);
}
/*
add_proc_to_queue is called by wakeup, inituser, fork.
wakeup: SLEEP -> RUNNABLE
inituser, fork: USED -> RUNNABLE
*/
void 
add_proc_to_queue(struct proc *p)
{
  acquire(&schedule_lock);
  if (schedule_mode == FCFS_MODE) push(&fcfs, p);
  else
  {
    p->level = 0;
    p->priority = 3;
    p->time_quantum = p->level * 2 + 1;
    push(mlfq + p->level, p);
  }
  release(&schedule_lock);
}

void
my_switching(struct proc *p, struct cpu *c)
{
  p->state = RUNNING;
  c->proc = p;
  swtch(&c->context, &p->context);
  c->proc = 0;
  if(p->state == RUNNABLE) my_queueing(p);
  release(&p->lock);
  acquire(&priority_boost_lock);
  if(priority_boost) priority_boosting();
  priority_boost = 0;
  release(&priority_boost_lock);
}

struct proc *
select_with_mlfq(void)
{
  for(int i = 0; i < 3; i++){
    while(!isEmpty(mlfq + i)){
        struct proc *t = pop(mlfq + i);
        acquire(&t->lock);
        if(t->state == RUNNABLE) {
        return t;
      }
      release(&t->lock);
    }
  }
  return 0;
}
struct proc *
select_with_fcfs(void)
{
  while(!isEmpty(&fcfs)){
    struct proc *t = pop(&fcfs);
    acquire(&t->lock);
    if(t->state == RUNNABLE) return t;
    release(&t->lock);
  }
  return 0;
}

void
my_scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  //w_sie(r_sie() & ~SIE_STIE);

  for(;;){
    intr_on();
    acquire(&schedule_lock);
    p = schedule_mode == FCFS_MODE ? select_with_fcfs() : select_with_mlfq();
    release(&schedule_lock);
    if(p) my_switching(p, c);
    else {
      intr_on();
      asm volatile("wfi");
    }
  }
}

uint64
sys_getlev(void)
{
  struct proc *p = myproc();
  acquire(&schedule_lock);
  if(schedule_mode == FCFS_MODE)
  {
    release(&schedule_lock);
    return 99;
  }
  release(&schedule_lock);
  return p->level;
}

uint64
sys_setpriority(void)
{
  int pid, priority;
  argint(0, &pid);
  argint(1, &priority);
  if(priority < 0 || priority > 3) return -2;

  struct proc *p = myproc();
  if(p->pid == pid){
    p->priority = priority;
    return 0;
  }
  struct proc * now = p;
  for(p = proc; p < &proc[NPROC]; p++)
  {
    if(p == now) continue;
    acquire(&p->lock);
    if(p->pid == pid)
    {
      p->priority = priority;
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

uint64
sys_fcfsmode(void)
{

  acquire(&schedule_lock);
  if(schedule_mode == FCFS_MODE)
  {
    release(&schedule_lock);
    return -1;
  }
  //w_sie(r_sie() & ~SIE_STIE);
  acquire(&tickslock);
  acquire(&fcfs.lock);
  fcfs.s = fcfs.e = 0;
  struct proc *now = myproc();
  for(struct proc* p=proc; p != &proc[NPROC]; p++) 
  {
    if(p != now) acquire(&p->lock);
    p->time_quantum = p->level = p->priority = -1;
    if(p->state == RUNNABLE) fcfs.p[fcfs.e++] = p;
    if(p != now) release(&p->lock);
  }
  release(&fcfs.lock);
  ticks = 0;
  release(&tickslock);
  schedule_mode = FCFS_MODE;
  release(&schedule_lock);
  w_stimecmp(r_time() + 1000000);
  return 0;
}

uint64
sys_mlfqmode(void)
{
  acquire(&schedule_lock);
  //w_sie(r_sie() | SIE_STIE);

  if(schedule_mode == MLFQ_MODE) 
  {
    release(&schedule_lock);
    return -1;
  }
  acquire(&tickslock);
  acquire(&fcfs.lock);
  for(int i = 0; i < 3; i++) 
  {
    acquire(&mlfq[i].lock);
    mlfq[i].s = mlfq[i].e = 0;
  }
  
  for(struct proc* p=proc; p != &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    p->time_quantum = 1;
    p->level = 0;
    p->priority = 3;
    release(&p->lock);
  }
  for(int i = fcfs.s; i != fcfs.e; i = (i + 1) & QMOD)
    if(fcfs.p[i]->state==RUNNABLE)
      mlfq->p[mlfq->e++] = fcfs.p[i];

  for(int i = 2; i >= 0; i--)
    release(&mlfq[i].lock);

  release(&fcfs.lock);
  schedule_mode = MLFQ_MODE;
  release(&schedule_lock);
  ticks = 0;
  release(&tickslock);
  w_stimecmp(r_time() + 1000000);
  return 0;
}
void
priority_boosting(){
  for(int i = 0; i < 3; i++) 
  {
    acquire(&mlfq[i].lock);
    mlfq[i].s = mlfq[i].e = 0;
  }
  for(struct proc *p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    p->priority = 3;
    p->level = 0;
    p->time_quantum = 1;
    if(p->state == RUNNABLE)
    {
      mlfq->p[mlfq->e++] = p;
    }
    release(&p->lock);
  }
  for(int i = 2; i >= 0; i--) release(&mlfq[i].lock);
}

uint64 
sys_yield(void)
{
  yield();
  return 0;
}