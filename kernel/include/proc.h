#ifndef __PROC_H__
#define __PROC_H__

#include "klib.h"
#include "vme.h"
#include "cte.h"
#include "sem.h"
#include "file.h"

#define KSTACK_SIZE 4096

typedef union {
  uint8_t stack[KSTACK_SIZE];
  struct {
    uint8_t pad[KSTACK_SIZE - sizeof(Context)];
    Context ctx;/*用户程序中断后一定会在内核栈顶构造中断上下文*/
  };
} kstack_t;

#define STACK_TOP(kstack) (&((kstack)->stack[KSTACK_SIZE]))
#define MAX_USEM 32
#define MAX_UFILE 32

//用这个结构体，代表一个进程所需要的资源
typedef struct proc {
  int pid;/*进程号，是进程的唯一标识*/
  enum {UNUSED, UNINIT, RUNNING, READY, ZOMBIE, BLOCKED} status;/*进程的状态*/
  PD *pgdir;/*维护这个进程虚拟地址空间的页目录*/
  size_t brk;/*指向这个进程的program break*/
  kstack_t *kstack;/*进程内核栈的栈底*/
  Context *ctx; // points to restore context for READY proc
  struct proc *parent; // Lab2-2，这个进程的父进程，但不是所有的都有父进程（比如内核进程和由内核进程直接创建的用户进程）
					   // 即记得判断NULL
  int child_num; // Lab2-2，体现父子进程关系，子进程数量
  int exit_code; // Lab2-3，代表僵尸进程的退出状态
  sem_t zombie_sem; // Lab2-4，信号量，管理自己的子僵尸进程
  usem_t *usems[MAX_USEM]; // Lab2-5，用这个数组表示该进程信号量的映射表(用户信号量表)
						   // 对应的信号量就是这个数组对应下标的对应项，用NULL表示这个编号没有对应的信号量
  file_t *files[MAX_UFILE]; // Lab3-1，维护用户打开的文件编号（文件描述符和文件映射关系的表），即用户打开文件表
  inode_t *cwd; // Lab3-2，表示这个进程的“当前目录”，这样访问文件就是从“当前目录”开始，而不必每次都从根目录开始
} proc_t;

void init_proc();
proc_t *proc_alloc();
void proc_free(proc_t *proc);
proc_t *proc_curr();
void proc_run(proc_t *proc) __attribute__((noreturn));
void proc_addready(proc_t *proc);
void proc_yield();
void proc_copycurr(proc_t *proc);
void proc_makezombie(proc_t *proc, int exitcode);
proc_t *proc_findzombie(proc_t *proc);
void proc_block();
int proc_allocusem(proc_t *proc);
usem_t *proc_getusem(proc_t *proc, int sem_id);
int proc_allocfile(proc_t *proc);
file_t *proc_getfile(proc_t *proc, int fd);

void schedule(Context *ctx);

#endif
