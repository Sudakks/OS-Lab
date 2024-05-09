#include "klib.h"
#include "cte.h"
#include "proc.h"

#define PROC_NUM 64

static __attribute__((used)) int next_pid = 1;

proc_t pcb[PROC_NUM];
static proc_t *curr = &pcb[0];
/*伪进程，即内核进程*/

void init_proc() {
  // Lab2-1, set status and pgdir
  /*用来设置pcb[0]的一些成员，以让它描述操作系统从启动到执行第一个用户程序之间的执行流所表示的“内核进程”*/
	curr->status = RUNNING;
	curr->pgdir = vm_curr();
  // Lab2-4, init zombie_sem
	sem_init(&pcb[0].zombie_sem, 0);
  // Lab3-2, set cwd
  curr->cwd = iopen("/", TYPE_NONE);
}

proc_t *proc_alloc() {
  // Lab2-1: find a unused pcb from pcb[1..PROC_NUM-1], return NULL if no such one
  int i;
  for(i = 1; i < PROC_NUM; i++)
  {
	if(pcb[i].status == UNUSED)
	{
		//不能假设找到的PCB所有成员都有初值0,所以都要初始化
		pcb[i].pid = next_pid;
		next_pid++;
		pcb[i].status = UNINIT;
		pcb[i].pgdir = (PD*)vm_alloc();
		pcb[i].brk = 0;
		pcb[i].kstack = (kstack_t*)kalloc();
		pcb[i].ctx = &(pcb[i].kstack->ctx);
		pcb[i].parent = NULL;
		pcb[i].child_num = 0;
		pcb[i].cwd = NULL;
		sem_init(&pcb[i].zombie_sem, 0);
		for(int k = 0; k < MAX_USEM; k++)
			pcb[i].usems[k] = NULL;
		//用户打开文件表init
		for(int k = 0; k < MAX_UFILE; k++)
			pcb[i].files[k] = NULL;
		return &pcb[i];
	}
  }
  return NULL;//表示没有空闲PCB
  //TODO();
  // init ALL attributes of the pcb
}

void proc_free(proc_t *proc) {
  // Lab2-1: free proc's pgdir and kstack and mark it UNUSED
  assert(proc != curr);
  proc->status = UNUSED;
  assert(proc->status != RUNNING);
  //TODO();
}

proc_t *proc_curr() {
  return curr;
}

void proc_run(proc_t *proc) {
  proc->status = RUNNING;
  curr = proc;
  set_cr3(proc->pgdir);
  set_tss(KSEL(SEG_KDATA), (uint32_t)STACK_TOP(proc->kstack));
  //ctx指向能让这个进程开始执行的中断上下文
  irq_iret(proc->ctx);
}

void proc_addready(proc_t *proc) {
  // Lab2-1: mark proc READY
  proc->status = READY;
}

void proc_yield() {
  // Lab2-1: mark curr proc READY, then int $0x81
  // 进行进程切换
  curr->status = READY;
  INT(0x81);
}

void proc_copycurr(proc_t *proc) {
  // Lab2-2: copy curr proc
  // 复制当前进程的地址空间和上下文的状态到proc这个进程中
  vm_copycurr(proc->pgdir);
  proc->brk = curr->brk;
  proc->kstack->ctx = curr->kstack->ctx;
  //proc->ctx = &(curr->kstack->ctx);
  proc->ctx->eax = 0;
  proc->parent = curr;
  curr->child_num++;
  // Lab2-5: dup opened usems
  for(int i = 0; i < MAX_USEM; i++)
  {
	  proc->usems[i] = curr->usems[i];
	  if(proc->usems[i] != NULL)
		usem_dup(curr->usems[i]);
  }
  // Lab3-1: dup opened files
  for(int i = 0; i < MAX_UFILE; i++)
  {
	  proc->files[i] = curr->files[i];
	  if(proc->files[i] != NULL)
		  fdup(curr->files[i]);
  }
  // Lab3-2: dup cwd
  proc->cwd = idup(curr->cwd);
  //TODO();
}

void proc_makezombie(proc_t *proc, int exitcode) {
  // Lab2-3: mark proc ZOMBIE and record exitcode, set children's parent to NULL
  proc->status = ZOMBIE;
  proc->exit_code = exitcode;
  for(int i = 1; i < PROC_NUM; i++)
  {
	  if(pcb[i].parent == proc)
		  pcb[i].parent = NULL;
  }
  //改写进程等待
  if(proc->parent != NULL)
	sem_v(&(proc->parent->zombie_sem));
  // Lab2-5: close opened usem
  for(int i = 0; i < MAX_USEM; i++)
  {
	  if(proc->usems[i] != NULL)
		usem_close(proc->usems[i]);
  }
  // Lab3-1: close opened files
  for(int i = 0; i < MAX_UFILE; i++)
  {
	  if(proc->files[i] != NULL)
		  fclose(proc->files[i]);
  }
  // Lab3-2: close cwd
  iclose(proc->cwd);
  //TODO();
}

proc_t *proc_findzombie(proc_t *proc) {
  // Lab2-3: find a ZOMBIE whose parent is proc, return NULL if none
  for(int i = 1; i < PROC_NUM; i++)
  {
	  if(pcb[i].status == ZOMBIE && pcb[i].parent == proc)
		  return &pcb[i];
  }
  return NULL;
  //TODO();
}

void proc_block() {
  // Lab2-4: mark curr proc BLOCKED, then int $0x81
  curr->status = BLOCKED;
  INT(0x81);
}

//用来管理进程的用户信号量表
int proc_allocusem(proc_t *proc) {
  // Lab2-5: find a free slot in proc->usems, return its index, or -1 if none
  for(int i = 0; i < MAX_USEM; i++)
  {
	if(proc->usems[i] == NULL)
		return i;
  }
  return -1;
  //TODO();
}

usem_t *proc_getusem(proc_t *proc, int sem_id) {
  // Lab2-5: return proc->usems[sem_id], or NULL if sem_id out of bound
  if(sem_id < 0 || sem_id >= MAX_USEM)
	  return NULL;
  return proc->usems[sem_id];
  //TODO();
}

int proc_allocfile(proc_t *proc) {
  // Lab3-1: find a free slot in proc->files, return its index, or -1 if none
  for(int i = 0; i < MAX_UFILE; i++)
  {
	  if(proc->files[i] == NULL)
	  {
		  //找到空闲下标
		  return i;
	  }
  }
  return -1;//没找到
  //TODO();
}

file_t *proc_getfile(proc_t *proc, int fd) {
  // Lab3-1: return proc->files[fd], or NULL if fd out of bound
  if(fd < 0 || fd >= MAX_UFILE)
	  return NULL;
  return proc->files[fd];
  //TODO();
}

void schedule(Context *ctx) {
  // Lab2-1: save ctx to curr->ctx, then find a READY proc and run it
  // 0x81号中断的处理函数
	curr->ctx = ctx;
	//记录保存当前进程状态的中断上下文的位置
	for(int i = 0; i < PROC_NUM; i++)
	{
		if(&pcb[i] == curr)
		{
			for(int j = i + 1; ; j = (j + 1) % PROC_NUM)
			{
				if(pcb[j].status == READY)
				{
					//找到可以切换的进程了
					proc_run(&pcb[j]);
					curr = &pcb[j];
					irq_iret(curr->ctx);
					return;
				}
			}
		}
	}
	  //TODO();
}
