#include "klib.h"
#include "vme.h"
#include "proc.h"

static TSS32 tss;

void init_gdt() {
  static SegDesc gdt[NR_SEG];
  gdt[SEG_KCODE] = SEG32(STA_X | STA_R,   0,     0xffffffff, DPL_KERN);
  gdt[SEG_KDATA] = SEG32(STA_W,           0,     0xffffffff, DPL_KERN);
  gdt[SEG_UCODE] = SEG32(STA_X | STA_R,   0,     0xffffffff, DPL_USER);
  gdt[SEG_UDATA] = SEG32(STA_W,           0,     0xffffffff, DPL_USER);
  gdt[SEG_TSS]   = SEG16(STS_T32A,     &tss,  sizeof(tss)-1, DPL_KERN);
  set_gdt(gdt, sizeof(gdt[0]) * NR_SEG);
  set_tr(KSEL(SEG_TSS));
}

void set_tss(uint32_t ss0, uint32_t esp0) {
  tss.ss0 = ss0;
  tss.esp0 = esp0;
}

static PD kpd;
static PT kpt[PHY_MEM / PT_SIZE] __attribute__((used));

typedef union free_page {
  union free_page *next;
  char buf[PGSIZE];
} page_t;

page_t *free_page_list;
void* alloc_mem_ptr;
void init_page() {
  extern char end;
  panic_on((size_t)(&end) >= KER_MEM - PGSIZE, "Kernel too big (MLE)");
  static_assert(sizeof(PTE) == 4, "PTE must be 4 bytes");
  static_assert(sizeof(PDE) == 4, "PDE must be 4 bytes");
  static_assert(sizeof(PT) == PGSIZE, "PT must be one page");
  static_assert(sizeof(PD) == PGSIZE, "PD must be one page");
  // Lab1-4: init kpd and kpt, identity mapping of [0 (or 4096), PHY_MEM)
  uint32_t nr_pt = PHY_MEM / PT_SIZE;
  for(uint32_t i = 0; i < nr_pt; i++)
	  kpd.pde[i].val = MAKE_PDE(&kpt[i], 3);
  for(uint32_t i = 0; i < nr_pt; i++)
  {
	  for(uint32_t j = 0; j < NR_PTE; j++)
	  {
		  size_t addr  = (i << DIR_SHIFT) | (j << TBL_SHIFT);
		  assert(addr >= 0 && addr < PHY_MEM);
		  kpt[i].pte[j].val = MAKE_PTE(addr, 3);
	  }
  }
  //TODO();
  kpt[0].pte[0].val = 0;
  set_cr3(&kpd);
  set_cr0(get_cr0() | CR0_PG);
  // Lab1-4: init free memory at [KER_MEM, PHY_MEM), a heap for kernel
  //TODO();
  /*
  page_t* new_page;
  for(uint32_t now_add = KER_MEM; now_add < PHY_MEM; now_add += PGSIZE)
  {
	  new_page = (page_t*)(PAGE_DOWN(now_add));
	  if(free_page_list == NULL)
	  {//这是头指针
		  new_page->next = NULL;
		  free_page_list = new_page;
	  }
	  else
	  {
		  new_page->next = free_page_list;
		  free_page_list = new_page;
	  }
  }
  */
	alloc_mem_ptr = (void*)KER_MEM;
}

bool check_valid(uint32_t va)
{
	PD *pgdir = (PD*)PAGE_DOWN(get_cr3());
	int pd_index = ADDR2DIR(va); // 计算“页目录号”
	PDE *pde = &(pgdir->pde[pd_index]); // 找到对应的页目录项
    PT *pt = PDE2PT(*pde); // 根据PDE找页表的地址
	int pt_index = ADDR2TBL(va); // 计算“页表号”
	PTE *pte = &(pt->pte[pt_index]); // 找到对应的页表项
	return (pte->present == 1) ? 1 : 0;
}

void *kalloc() {
  // Lab1-4: alloc a page from kernel heap, abort when heap empty
  //TODO();
  /*
  if(free_page_list == NULL)
	  assert(0);
  page_t* alloc_page = free_page_list;
  free_page_list = alloc_page->next;
  assert(check_valid((uint32_t)alloc_page) == 1);
  //用来检测是不是p位有效，即是分配页
  memset((char*)alloc_page, 0, PGSIZE);
  return alloc_page;
  */
  if((size_t)alloc_mem_ptr + PGSIZE >= PHY_MEM)
	  return NULL;
  void* ret = alloc_mem_ptr;
  alloc_mem_ptr = (char*)alloc_mem_ptr + PGSIZE;
  return ret;
}

void kfree(void *ptr) {
  // Lab1-4: free a page to kernel heap
  // you can just do nothing :)
  //TODO();
  /*
  page_t * new_page = (page_t*)(PAGE_DOWN((uint32_t)ptr));
  memset((char*)new_page, 0, PGSIZE);
  new_page->next = free_page_list;
  free_page_list = new_page;
  */
}

PD *vm_alloc() {
  // Lab1-4: alloc a new pgdir, map memory under PHY_MEM identityly
  // 返回用户页目录，并映射[0, PHY_MEM)这一块区域的内容（作为内核的映射）
  PD* new_pdir = (PD*)kalloc();
  int nr_pt = 32;
  memset(new_pdir, 0, sizeof(PDE) * NR_PDE);
  for(uint32_t i = 0; i < nr_pt; i++)
	  new_pdir->pde[i].val = MAKE_PDE(&kpt[i], 3);
  return new_pdir;
  //TODO();
}

void vm_teardown(PD *pgdir) {
  // Lab1-4: free all pages mapping above PHY_MEM in pgdir, then free itself
  // you can just do nothing :)
  //TODO();
}

PD *vm_curr() {
  return (PD*)PAGE_DOWN(get_cr3());
}

PTE *vm_walkpte(PD *pgdir, size_t va, int prot) {
  // Lab1-4: return the pointer of PTE which match va
  // if not exist (PDE of va is empty) and prot&1, alloc PT and fill the PDE
  // if not exist (PDE of va is empty) and !(prot&1), return NULL
  // remember to let pde's prot |= prot, but not pte
  assert(va >= PHY_MEM);
  int pd_index = ADDR2DIR(va); 
  PDE *pde = &(pgdir->pde[pd_index]); 
  if(pde->present == 0 && prot == 0)
	  return NULL;
  else if(pde->present == 0 && prot != 0)
  {
	  //初始化pte，即不保证有效位为1
	  PT* pt = kalloc();
	  for(uint32_t i = 0; i < NR_PTE; i++)
		  pt->pte[i].val = 0;
	  pde->val = MAKE_PDE(pt, prot);
	  int pt_index = ADDR2TBL(va);
	  PTE* pte = &(pt->pte[pt_index]);
	  return pte;
  }
  else
  {
	  PT* pt = PDE2PT(*pde);
	  int pt_index = ADDR2TBL(va);
	  PTE* pte = &(pt->pte[pt_index]);
	  return pte;
  }
	assert((prot & ~7) == 0);
  //TODO();
}

void *vm_walk(PD *pgdir, size_t va, int prot) {
  // Lab1-4: translate va to pa
  // if prot&1 and prot voilation ((pte->val & prot & 7) != prot), call vm_pgfault
  // if va is not mapped and !(prot&1), return NULL
  //TODO();
  PTE* pte = vm_walkpte(pgdir, va, prot);
  if(!pte || !(prot & 1))
	  return NULL;
  void *page = PTE2PG(*pte);
  void *pa = (void*)((uint32_t)page | ADDR2OFF(va));
  return pa;
 }

void vm_map(PD *pgdir, size_t va, size_t len, int prot) {
  // Lab1-4: map [PAGE_DOWN(va), PAGE_UP(va+len)) at pgdir, with prot
  // if have already mapped pages, just let pte->prot |= prot
  assert(prot & PTE_P);
  //已经说明prot不可能为0
  assert((prot & ~7) == 0);
  size_t start = PAGE_DOWN(va);
  size_t end = PAGE_UP(va + len);
  assert(start >= PHY_MEM);
  assert(end >= start);

  for(size_t now = start; now < end; now += PGSIZE)
  {
	PTE* pte = vm_walkpte(pgdir, now, prot);
	assert(pte);
	pte->val = MAKE_PTE(kalloc(), prot);
	/*
	if(pte->present == 1)
	{//表示已经被映射过了
	  pte->val |= prot;
	}
	else
	  pte->val = MAKE_PTE(kalloc(), prot);
	  */
  }
  //TODO();
}

void vm_unmap(PD *pgdir, size_t va, size_t len) {
  // Lab1-4: unmap and free [va, va+len) at pgdir
  // you can just do nothing :)
  //assert(ADDR2OFF(va) == 0);
  //assert(ADDR2OFF(len) == 0);
  TODO();
}

void vm_copycurr(PD *pgdir) {
  // Lab2-2: copy memory mapped in curr pd to pgdir
  // 复制当前的虚拟空间地址到pgdir这个页目录里
  // pgdir只有[0,OHY_MEM)的恒等映射
  PD* now_pgdir = vm_curr();
  for(size_t va = PHY_MEM; va < USR_MEM; va += PGSIZE)
  {
	PTE *pte = vm_walkpte(now_pgdir, va, /*TODO*/0);	  
	if(!pte || pte->present == 0)
		continue;
	//权限！！！和原来pte的设置成一样的
	vm_map(pgdir, va, PGSIZE, (pte->val & 0x7));
	void* pa = vm_walk(pgdir, va, (pte->val & 0x7));
	memcpy(pa, (void*)va, PGSIZE);
  }
  //TODO();
}

void vm_pgfault(size_t va, int errcode) {
  printf("pagefault @ 0x%p, errcode = %d\n", va, errcode);
  panic("pgfault");
}
