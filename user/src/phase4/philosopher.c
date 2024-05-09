#include "philosopher.h"

// TODO: define some sem if you need
int chopstick[PHI_NUM];
int mutex;
void init() {
  // init some sem if you need
  mutex = sem_open(1);
  for(int i = 0; i < PHI_NUM; i++)
	  chopstick[i] = sem_open(1);//初始都是1根筷子
  //for(int i = 0; i < PHI_NUM; i++)
    //  philosopher(i);
  //TODO();
}

void philosopher(int id) {
  // implement philosopher, remember to call `eat` and `think`
  while (1) {
	P(mutex);
	P(chopstick[id]);
	P(chopstick[(id + 1 ) % PHI_NUM]);
	V(mutex);
	eat(id);
	V(chopstick[id]);
	V(chopstick[(id + 1) % PHI_NUM]);
	think(id);
    //TODO();
  }
}
