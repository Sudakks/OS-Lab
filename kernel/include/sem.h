#ifndef __SEM_H__
#define __SEM_H__

#include "klib.h"

typedef struct sem {
  int value;//为正，表示当前资源还有这么多个，为负表示当前没有资源且还有这么多进程在等
  list_t wait_list;//进程队列
} sem_t;
/*
 *信号量
*/

void sem_init(sem_t *sem, int value);
void sem_p(sem_t *sem);//申请资源
void sem_v(sem_t *sem);//释放资源

typedef struct usem {
  sem_t sem;
  int ref;//代表当前有几个用户进程持有这个信号量
} usem_t;
//表示给用户程序使用的信号量

usem_t *usem_alloc(int value);
usem_t *usem_dup(usem_t *usem);
void usem_close(usem_t *usem);

#endif
