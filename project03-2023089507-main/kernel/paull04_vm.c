#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "spinlock.h"

#define N_PHY_PAGES ((PHYSTOP) >> PGSHIFT)


struct spinlock pgref_lock;
static int pgref[N_PHY_PAGES];
int init_flag;

void init_pgref_lock(){
  initlock(&pgref_lock, "pgref Lock");
}
void print_all_activated_page(){
  for(int i = 0; i < N_PHY_PAGES; i++) if(pgref[i]) printf("pgref[%d]=%d\n", i, pgref[i]);
}

void increment_ref(uint64 pa){
  if (pa >= PHYSTOP) {
    panic("increment_ref: invalid pa");
  }
  acquire(&pgref_lock);
  ++pgref[pa >> PGSHIFT];
  //printf("inc: pgref[%lu]=%d\n", pa, r);
  release(&pgref_lock);
}
void decrement_ref(uint64 pa){
  if (pa >= PHYSTOP) {
    panic("decrement_ref: invalid pa");
  }
  acquire(&pgref_lock);
  int r = --pgref[pa >> PGSHIFT];
  //printf("dec: pgref[%lu]=%d\n", pa, r);
  release(&pgref_lock);
  if(r == 0) kfree((void *)pa);
}
int get_ref(uint64 pa){
  if (pa >= PHYSTOP) {
    panic("decrement_ref: invalid pa");
  }
  acquire(&pgref_lock);
  int r = pgref[pa >> PGSHIFT];
  release(&pgref_lock);
  return r;
}

uint64 page_change(pagetable_t pagetable, pte_t *pte, uint64 va0){
  uint64 oldpa = PTE2PA(*pte);
  uint64 pa = oldpa;
  if(get_ref(oldpa) > 1){
    pa = (uint64)kalloc();
    if(pa == 0) return 0;
    memmove((void*)pa, (void*)oldpa, PGSIZE);
    decrement_ref(oldpa);
    increment_ref(pa);
  }
  uint64 flags = PTE_FLAGS(*pte);
  flags |= PTE_W;
  flags &= ~PTE_COW;
  *pte = PA2PTE(pa) | flags;
  sfence_vma();
  return pa;
}

uint64 re_page(pagetable_t pagetable, pte_t *pte, uint64 va0){
  return (*pte & PTE_W)?
    PTE2PA(*pte) :
    page_change(pagetable, pte, va0);
}

int
pagefault_handler(pagetable_t pagetable, uint64 va){
  pte_t *pte;
  va = PGROUNDDOWN(va);
  if ((pte = walk(pagetable, va, 0)) == 0 || (*pte & PTE_V) == 0) return -1;
  else if((*pte & PTE_COW) == 0) return -1;
  if(page_change(pagetable, pte, va) == 0) return -1;
  return 0;
}