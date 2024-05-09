#include "klib.h"
#include "serial.h"
#include "vme.h"
#include "cte.h"
#include "loader.h"
#include "fs.h"
#include "proc.h"
#include "timer.h"
#include "dev.h"

void init_user_and_go();

int main() {
  init_gdt();
  init_serial();
  init_fs();
  init_page(); // uncomment me at Lab1-4
  init_cte(); // uncomment me at Lab1-5
  init_timer(); // uncomment me at Lab1-7
  init_proc(); // uncomment me at Lab2-1
  init_dev(); // uncomment me at Lab3-1
  printf("Hello from OS!\n");
  init_user_and_go();
  panic("should never come back");
}

void init_user_and_go() {
  // Lab1-2: ((void(*)())eip)();
  // Lab1-4: pdgir, stack_switch_call
  // Lab1-6: ctx, irq_iret
  // Lab1-8: argv
  // Lab2-1: proc
  // Lab3-2: add cwd
	/*
 	PD *pgdir = vm_alloc();
    Context ctx;
	//char *argv[] = {"echo", "hello", "world", NULL};
	//!!!remember：注意参数的设置啊啊啊啊！！！
	//前面一个是程序的名字，后面一个一定是NULL！！！
	//之前似乎也是在这里错了，哭
	char *argv[] = {"sh1", NULL};
	assert(load_user(pgdir, &ctx, "sh1", argv) == 0);
	set_cr3(pgdir);
	set_tss(KSEL(SEG_KDATA), (uint32_t)kalloc() + PGSIZE);
	irq_iret(&ctx);
	*/

	/*让内核程序在最后把中断打开，可以处理中断*/
	/*而proc_yield只能在关中断下使用，所以等到时钟中断再切换走*/
	proc_t *proc = proc_alloc();
	proc->cwd = iopen("/", TYPE_NONE);
	assert(proc);
	//char *argv[] = {"ping1", "114514", NULL};
	//char *argv[] = {"ping3", "114514", "1919810", NULL};
	char *argv[] ={"sh", NULL}; 
	assert(load_user(proc->pgdir, proc->ctx, "sh", argv) == 0);
	proc_addready(proc);
	/*
	proc = proc_alloc();
	assert(proc);
	argv[1] = "1919810";
	assert(load_user(proc->pgdir, proc->ctx, "ping1", argv) == 0);
	proc_addready(proc);
	*/
	sti();
	while (1) ;
}
