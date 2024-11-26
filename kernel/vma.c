#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"
#include "proc.h"
#include "memlayout.h"
#include "fcntl.h"

#define VMA_CNT 16
const uint64 BAD_ADDR = 0xffffffffffffffff;

struct vma all_vma[VMA_CNT];

struct {
	struct spinlock lk;
	struct vma *free_vma;
} vma_list;

int
vma_copy(struct proc *p, struct proc *np)
{
	if(np->pVMA)
		return -1;

	if(p->pVMA && !p->pVMA->f)
		return -1;

	struct vma *list = p->pVMA;
	struct vma *res = 0;
	struct vma *tail = 0;

	while(list){
		struct vma *tmp = vma_alloc();
		
		if(!tmp){
			// Don't have enough VMA entries.
			goto err_copy;
		}

		*tmp = *list;
		tmp->f = filedup(list->f);

		if(!res){
			// First add new entry.
			res = vma_map(np->pagetable, res, tmp);
			tail = res;
		} else {
			// Tail add new entry.
			tail->next = vma_map(np->pagetable, tail->next, tmp);
			tail = tail->next;
		}

		list = list->next;
	}

	np->pVMA = res;

	return 0;

err_copy:
	while(res){
		res = vma_unmap(np->pagetable, res, res);
	}
	return -1;
}

int
vma_load(struct proc *pc, uint64 va)
{
	struct vma *lst = pc->pVMA;

	begin_op();

	while(lst){
		if(va >= lst->bgAddr && va < lst->edAddr){
			pte_t *pte = 0;
			int xperm = PTE_U;
			int test = 0;

			struct file *f = lst->f;

			va = PGROUNDDOWN(va);
			uint64 mpOffset = lst->offset + (va - lst->bgAddr);

			if((pte = walk(pc->pagetable, va, 1)) == 0)
				panic("vma_load: walk fail");

    	if(!(*pte & PTE_M) || !(*pte & PTE_V))
      	panic("vma_load: not maped");

			if(PTE2PA(*pte))
				panic("vma_load: already loaded");

			void *pa = kalloc();

			if(pa == 0)
				panic("vma_load: kalloc fail");

			memset(pa, test, PGSIZE);

			// Load physical memmory
    	*pte = PA2PTE(pa) | *pte | xperm;

    	ilock(f->ip);
    	test = readi(f->ip, 1, (uint64)va, mpOffset, PGSIZE);
    	iunlock(f->ip);

			end_op();

			return 0;
		}
		lst = lst->next;
	}

	end_op();

	return -1;
}

int
vma_unload(pagetable_t pagetable, uint64 va, struct vma *pVMA)
{
	pte_t *pte = 0;
	struct file *f = pVMA->f;

	va = PGROUNDDOWN(va);
	uint64 mpOffset = pVMA->offset + (va - pVMA->bgAddr);

	if((pte = walk(pagetable, va, 1)) == 0)
		panic("vma_unload: walk fail");

	if(!(*pte & PTE_M) || !(*pte & PTE_V))
		panic("vma_unload: not maped");

	if(!(*pte & PTE_U)){
		// no actual physical mem
		*pte = 0;
		return 0;
	}

	if(!PTE2PA(*pte))
		panic("vma_unload: PTE2PA fail");

	if(f && (*pte & PTE_D) && (pVMA->flags & MAP_SHARED)){
		ilock(f->ip);
		begin_op();
		if(writei(f->ip, 1, va, mpOffset, PGSIZE) != PGSIZE)
			panic("vma_unload: writei fail");
		end_op();
		iunlock(f->ip);
	}

	kfree((void*)PTE2PA(*pte));
	*pte=0;

	return 0;
}

// Unmap address range represented by pVMA
struct vma *
vma_unmap(pagetable_t pagetable, struct vma *list, struct vma *pVMA)
{
	if(pVMA == 0){
		return list;
	}

	struct vma *lVMA = list;
	struct vma *preVMA = list;
	while(lVMA){
		if(pVMA->bgAddr >= lVMA->bgAddr && pVMA->bgAddr < lVMA->edAddr){
			uint64 va = PGROUNDDOWN(pVMA->bgAddr);
			uint64 fa = PGROUNDUP(pVMA->edAddr);

			for(uint64 v = va;v < fa; v += PGSIZE){
				if(vma_unload(pagetable, v, lVMA) != 0)
					panic("vma_unmap: vam_unload fail");
			}

			if(va == lVMA->bgAddr){
				lVMA->bgAddr = fa;
			} else {
				lVMA->edAddr = va;
			}

			if(lVMA->bgAddr == lVMA->edAddr){
				fileclose(lVMA->f);
				if(lVMA != preVMA){
					preVMA->next = vma_free(lVMA);
					return list;
				} else {
					return vma_free(lVMA);
				}
			}
		}
		if(lVMA != preVMA){
			preVMA = lVMA;
		}
		lVMA = lVMA->next;
	}

	return list;
}

struct vma *
vma_map(pagetable_t pagetable, struct vma *list, struct vma *pVMA)
{
	uint64 va = pVMA->bgAddr;
	uint64 fa = pVMA->edAddr;
	int xperm = PTE_M;

	if(pVMA->prot & PROT_READ)
		xperm |= PTE_R;
	if(pVMA->prot & PROT_WRITE)
		xperm |= PTE_W;
	if(pVMA->prot & PROT_EXEC)
		xperm |= PTE_X;

	for(;va < fa; va += PGSIZE){
		if(mappages(pagetable, va, PGSIZE, 0, xperm) != 0)
			panic("vma_map: mappages fail");
	}

	pVMA->next = list;

	return pVMA;
}

struct vma*
vma_alloc(void)
{
	struct vma* res = 0;

	acquire(&vma_list.lk);

	if(vma_list.free_vma != 0){
		res = vma_list.free_vma;
		vma_list.free_vma = vma_list.free_vma->next;

		release(&vma_list.lk);

		memset(res, 0, sizeof(struct vma));
	}

	return res;
}

struct vma*
vma_free(struct vma *pVMA)
{
	struct vma *res = 0;

	if(pVMA){
		acquire(&vma_list.lk);

		res = pVMA->next;
		pVMA->next = vma_list.free_vma;
		vma_list.free_vma = pVMA;

		release(&vma_list.lk);
	} else {
		panic("vma_free args");
	}

	return res;
}

void
vma_init(void)
{
	initlock(&vma_list.lk, "vma_list");
	vma_list.free_vma = all_vma;

	for(int i = 0; i < VMA_CNT; ++i){
		memset((void*)(all_vma + i), 0, sizeof(struct vma));

		initlock(&all_vma[i].lk, "vma");
		all_vma[i].bgAddr = BAD_ADDR;
		all_vma[i].edAddr = BAD_ADDR;
		if(i + 1 < VMA_CNT){
			all_vma[i].next = all_vma + i + 1;
		}	
	}
}

void*
mmap(void *addr, size_t length, int prot, int flags, struct file *f, uint64 offset)
{
  if(!f || f->readable == 0)
    goto err_mmap;

	struct proc *pc = myproc();
	if(pc == 0)
		goto err_mmap;

	struct vma *pVMA = vma_alloc();
	if(pVMA == 0)
		goto err_mmap;

	pVMA->flags = flags;
	pVMA->prot = prot;
	pVMA->offset = offset;
	pVMA->f = f;

	struct vma *list = pc->pVMA;
	uint64 va = BAD_ADDR;
	uint64 fa = BAD_ADDR;
	struct vma *tmp = list;

	while(tmp){
		if(tmp->next && tmp->bgAddr - tmp->next->edAddr >= length){
			va = tmp->next->edAddr;
			break;
		}
		tmp = tmp->next;
	}

	if(va == BAD_ADDR){
		if(list){
			// All alocated space used, add a new one
			va = PGROUNDUP(list->edAddr);
		} else {
			// Begin VMA list
			va = MMAPFRAME;
		}
	}

	fa = PGROUNDUP(va + length);

	pVMA->bgAddr = va;
	pVMA->edAddr = fa;

	if(tmp){
		tmp->next = vma_map(pc->pagetable, tmp->next, pVMA);
	} else{
		pc->pVMA = vma_map(pc->pagetable, list, pVMA);
	}

	return (void*)va;

err_mmap:
	return (void*)BAD_ADDR;
}

void
munmap(void *addr, size_t length)
{
	struct proc *pc = myproc();
	if(pc == 0)
		goto err_munmap;

	struct vma *pVMA = vma_alloc();
	if(pVMA == 0)
		goto err_munmap;

	pVMA->bgAddr = PGROUNDDOWN((uint64)addr);
	pVMA->edAddr = PGROUNDUP((uint64)addr + length);

	pc->pVMA = vma_unmap(pc->pagetable, pc->pVMA, pVMA);

	vma_free(pVMA);

err_munmap:
	return;
}
