#include "klib.h"
#include "sem.h"
#include "proc.h"

void sem_init(sem_t *sem, int value) {
  sem->value = value;
  list_init(&sem->wait_list);
}

void sem_p(sem_t *sem) {
  // Lab2-4: dec sem's value, if value<0, add curr proc to waitlist and block it
  sem->value--;
  if(sem->value < 0)
  {
	  list_enqueue(&sem->wait_list, proc_curr());
	  proc_block();
  }
  //TODO();
}

void sem_v(sem_t *sem) {
  // Lab2-4: inc sem's value, if value<=0, dequeue a proc from waitlist and ready it
  sem->value++;
  if(sem->value <= 0)
  {
	  proc_t* now = (proc_t*)list_dequeue(&sem->wait_list);
	  proc_addready(now);
  }
  //TODO();
}

#define USER_SEM_NUM 128
static usem_t user_sem[USER_SEM_NUM] __attribute__((used));
//表示给用户程序用的信号量有这么多个
//存放用户程序可以使用的信号量

//这3个API都是针对于sem自身而言的
usem_t *usem_alloc(int value) {
  // Lab2-5: find a usem whose ref==0, init it, inc ref and return it, return NULL if none
  for(int i = 0; i < USER_SEM_NUM; i++)
  {
	  if(user_sem[i].ref == 0)
	  {
		  sem_init(&user_sem[i].sem, value);
		  user_sem[i].ref++;
		  return &user_sem[i];
	  }
  }
  return NULL;
  //TODO();
}

usem_t *usem_dup(usem_t *usem) {
  // Lab2-5: inc usem's ref
  usem->ref++;
  return usem;
  //主要是复制usem时使用的
  //TODO();
}

void usem_close(usem_t *usem) {
  // Lab2-5: dec usem's ref
  usem->ref--;
  //TODO();
}


