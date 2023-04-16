#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 uaddr;
  uint64 vaddr, daddr;
  int cnt, idx = 0;
  unsigned int bitmask = 0;

  argaddr(0, &vaddr);
  argint(1, &cnt);
  argaddr(2, &uaddr);

  // printf("============PAGE TABLE==========\n");
  // vmprint(myproc()->pagetable, 2);
  // printf("================================\n");

  // Set the limit of the number of pages that can be scanned to sizeof(unsigned int) * 8.
  if(cnt > sizeof(unsigned int) * 8){
    panic("pgaccess1");
  }

  daddr = PGROUNDUP(vaddr+cnt*PGSIZE-1);
  vaddr = PGROUNDDOWN(vaddr);
  for(uint64 i = vaddr; i < daddr; i += PGSIZE, ++ idx){
    pte_t *pte = walk(myproc()->pagetable, i, 0);

    // If the virtual address is not mapped, panic.
    if(!pte)
      panic("pgaccess2");

    if(*pte & PTE_A){
      *pte ^= PTE_A;
      bitmask |= (1 << idx);
    }
  }

  if(copyout(myproc()->pagetable, uaddr, (char*)&bitmask, sizeof(bitmask)) < 0)
    panic("paaccess3");

  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
