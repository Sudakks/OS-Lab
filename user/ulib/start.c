#include "ulib.h"

int main(int argc, char *argv[]);

void _start(int argc, char *argv[]) {
	//对argv取下标会得到一个char*，即字符串
  exit(main(argc, argv));
}

int abort(const char *file, int line, const char *info) {
  printf("Abort @ [%s:%d] %s\n", file, line, info ? info : "");
  //exit(1); // TODO: uncomment me at Lab2-3
  while (1) ;
}
