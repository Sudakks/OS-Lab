#include "klib.h"
#include "cte.h"
#include "sysnum.h"
#include "vme.h"
#include "serial.h"
#include "loader.h"
#include "proc.h"
#include "timer.h"
#include "file.h"

typedef int (*syshandle_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

extern void *syscall_handle[NR_SYS];

void do_syscall(Context *ctx) {
  // TODO: Lab1-5 call specific syscall handle and set ctx register
  int sysnum = ctx->eax;
  uint32_t arg1 = ctx->ebx;
  uint32_t arg2 = ctx->ecx;
  uint32_t arg3 = ctx->edx;
  uint32_t arg4 = ctx->esi;
  uint32_t arg5 = ctx->edi;
  int res;
  if (sysnum < 0 || sysnum >= NR_SYS) {
    res = -1;
  } else {
    res = ((syshandle_t)(syscall_handle[sysnum]))(arg1, arg2, arg3, arg4, arg5);
  }
  ctx->eax = res;
}

int sys_write(int fd, const void *buf, size_t count) {
  // TODO: rewrite me at Lab3-1
  file_t *now_file = proc_getfile(proc_curr(), fd);
  if(now_file == NULL)
	  return -1;
  int ret = fwrite(now_file, buf, count);
  return ret;
  //return serial_write(buf, count);
}

int sys_read(int fd, void *buf, size_t count) {
  // TODO: rewrite me at Lab3-1
  file_t *now_file = proc_getfile(proc_curr(), fd);
  if(now_file == NULL)
	  return -1;
  int ret = fread(now_file, buf, count);
  return ret;

  //return serial_read(buf, count);
}

int sys_brk(void *addr) {
  // TODO: Lab1-5
  /*
  static size_t brk = 0; // use brk of proc instead of this in Lab2-1
						 // 操作系统认为用户程序的program break在哪，一开始为0表示用户程序还没告诉os program break在哪
  size_t new_brk = PAGE_UP(addr);
  if (brk == 0) {
    brk = new_brk;
  } else if (new_brk > brk) {
	  //按页管理，所以把传入的地址按页向上取整作为new_brk
	vm_map(vm_curr(), brk, new_brk - brk, 7);
	brk = new_brk;
    //TODO();
  } else if (new_brk < brk) {
    // can just do nothing
  }
  return 0;
  */
//lab2-1 using current pcb->brk
  proc_t* proc = proc_curr();
  size_t new_brk = PAGE_UP(addr);
  if (proc->brk == 0) {
    proc->brk = new_brk;
  } else if (new_brk > proc->brk) {
	  //按页管理，所以把传入的地址按页向上取整作为new_brk
	vm_map(vm_curr(), proc->brk, new_brk - proc->brk, 7);
	proc->brk = new_brk;
    //TODO();
  } else if (new_brk < proc->brk) {
    // can just do nothing
  }
  return 0;
}

void sys_sleep(int ticks) {
	uint32_t now_tick = get_tick();
	while(get_tick() - now_tick < ticks)
	{
		//sti(); hlt(); cli();
		proc_yield();
	}
  //TODO(); // Lab1-7
}

int sys_exec(const char *path, char *const argv[]) {
	PD* pgdir = vm_alloc();
	assert(pgdir);
	Context ctx;
	int ret = load_user(pgdir, &ctx, path, argv);
	if(ret != 0)
	{
		//kfree(pgdir);
		return -1;
	}
	//PD* old_pgdir = vm_curr();
	set_cr3(pgdir);
	//lab2-1
	proc_t* now = proc_curr();
	now->pgdir = pgdir;
	//相当于把这个进程的pgdir切换到了这个程序的pgdir

	//kfree(old_pgdir);
	irq_iret(&ctx);
		//内核栈可以用之前那个不用换
	return 0;
  //TODO(); // Lab1-8, Lab2-1
}

int sys_getpid() {
	proc_t* now = proc_curr();
	return now->pid;
  //TODO(); // Lab2-1
}

void sys_yield() {
  proc_yield();
}

int sys_fork() {
	proc_t* now_proc = proc_alloc();
	if(!now_proc)
		return -1;
	proc_copycurr(now_proc);
	proc_addready(now_proc);
	return now_proc->pid;
  //TODO(); // Lab2-2
}

void sys_exit(int status) {
	//进程的第一次死亡
	proc_makezombie(proc_curr(), status);
	INT(0x81);
	assert(0);
	//while (1) proc_yield();
    //TODO(); // Lab2-3
}

int sys_wait(int *status) {
  //TODO(); // Lab2-3, Lab2-4
	if(proc_curr()->child_num == 0)
		return -1;
	proc_t *zo_child;
	/*
	while((zo_child = proc_findzombie(proc_curr())) == NULL)
		proc_yield();
	*/
	//2-4-2
	sem_p(&(proc_curr()->zombie_sem));
	assert((zo_child = proc_findzombie(proc_curr())) != NULL);
	proc_yield();
	int ret_pid = 0;
	if(status != NULL)	
	{
		//要保存状态
		*status = zo_child->exit_code;//这是退出状态 
	}
	ret_pid = zo_child->pid;
	proc_free(zo_child);
	proc_curr()->child_num--;
	return ret_pid;
  /*
  sys_sleep(250);
  return 0;
  */
}

int sys_sem_open(int value) {
	int id = proc_allocusem(proc_curr());
	//可用的信号量
	if(id == -1)
		return -1;
	usem_t * alloc_sem = usem_alloc(value);
	//用户可用的信号量表
	if(alloc_sem == NULL)
		return -1;
	proc_curr()->usems[id] = alloc_sem;
	return id;
  //TODO(); // Lab2-5
}

int sys_sem_p(int sem_id) {
	usem_t * now_sem = proc_getusem(proc_curr(), sem_id);
	if(now_sem == NULL)
		return -1;
	sem_p(&now_sem->sem);
	return 0;
  //TODO(); // Lab2-5
}

int sys_sem_v(int sem_id) {
	usem_t * now_sem = proc_getusem(proc_curr(), sem_id);
	if(now_sem == NULL)
		return -1;
	sem_v(&now_sem->sem);
	return 0;
  //TODO(); // Lab2-5
}

int sys_sem_close(int sem_id) {
	usem_t * now_sem = proc_getusem(proc_curr(), sem_id);
	if(now_sem == NULL)
		return -1;
	usem_close(now_sem);
	proc_curr()->usems[sem_id] = NULL;
	return 0;
  //TODO(); // Lab2-5
}

int sys_open(const char *path, int mode) {
	int fd = proc_allocfile(proc_curr());
	if(fd == -1)
		return -1;
	file_t *now_file = fopen(path, mode);
	if(now_file == NULL)
		return -1;
	proc_curr()->files[fd] = now_file;
	return fd;
  //TODO(); // Lab3-1
}

int sys_close(int fd) {
  file_t *now_file = proc_getfile(proc_curr(), fd);
  if(now_file == NULL)
	  return -1;
  fclose(now_file);
  proc_curr()->files[fd] = NULL;
  return 0;
  //TODO(); // Lab3-1
}

int sys_dup(int fd) {
	//用来复制文件描述符，让不同的文件描述符对应到相同的file_t
  int free_fd = proc_allocfile(proc_curr());
  file_t *now_file = proc_getfile(proc_curr(), fd);
  if(now_file == NULL || free_fd == -1)
	  return -1;
  proc_curr()->files[free_fd] = now_file;
  fdup(now_file);
  return free_fd;//返回找到的fd
  //TODO(); // Lab3-1
}

uint32_t sys_lseek(int fd, uint32_t off, int whence) {
  file_t *now_file = proc_getfile(proc_curr(), fd);
  if(now_file == NULL)
	  return -1;
  uint32_t ret = fseek(now_file, off, whence);
  return ret;
  //TODO(); // Lab3-1
}

int sys_fstat(int fd, struct stat *st) {
	//用来获取文件信息的
  file_t *now_file = proc_getfile(proc_curr(), fd);
  if(now_file == NULL)
	  return -1;
  if(now_file->type == TYPE_FILE)
  {
	st->type = itype(now_file->inode);
	st->size = isize(now_file->inode);
	st->node = ino(now_file->inode);
  }
  else if(now_file->type == TYPE_DEV)
  {
	  st->type = TYPE_DEV;
	  st->size = 0;
	  st->node = 0;
  }
  return 0;
  //还不确定这个return的值
  //TODO(); // Lab3-1
}

int sys_chdir(const char *path) {
  //TODO(); // Lab3-2
  //改变当前进程cwd为path这一条路径指向的目录，success为0
  inode_t *inode = iopen(path, TYPE_NONE);
  if(inode == NULL)
	  return -1;
  if(itype(inode) != TYPE_DIR)
  {
	  iclose(inode);
	  return -1;
  }
  iclose(proc_curr()->cwd);
  proc_curr()->cwd = inode;
  return 0;
}

int sys_unlink(const char *path) {
  return iremove(path);
}

// optional syscall

void *sys_mmap() {
  TODO();
}

void sys_munmap(void *addr) {
  TODO();
}

int sys_clone(void (*entry)(void*), void *stack, void *arg) {
  TODO();
}

int sys_kill(int pid) {
  TODO();
}

int sys_cv_open() {
  TODO();
}

int sys_cv_wait(int cv_id, int sem_id) {
  TODO();
}

int sys_cv_sig(int cv_id) {
  TODO();
}

int sys_cv_sigall(int cv_id) {
  TODO();
}

int sys_cv_close(int cv_id) {
  TODO();
}

int sys_pipe(int fd[2]) {
  TODO();
}

int sys_link(const char *oldpath, const char *newpath) {
  TODO();
}

int sys_symlink(const char *oldpath, const char *newpath) {
  TODO();
}

void *syscall_handle[NR_SYS] = {
  [SYS_write] = sys_write,
  [SYS_read] = sys_read,
  [SYS_brk] = sys_brk,
  [SYS_sleep] = sys_sleep,
  [SYS_exec] = sys_exec,
  [SYS_getpid] = sys_getpid,
  [SYS_yield] = sys_yield,
  [SYS_fork] = sys_fork,
  [SYS_exit] = sys_exit,
  [SYS_wait] = sys_wait,
  [SYS_sem_open] = sys_sem_open,
  [SYS_sem_p] = sys_sem_p,
  [SYS_sem_v] = sys_sem_v,
  [SYS_sem_close] = sys_sem_close,
  [SYS_open] = sys_open,
  [SYS_close] = sys_close,
  [SYS_dup] = sys_dup,
  [SYS_lseek] = sys_lseek,
  [SYS_fstat] = sys_fstat,
  [SYS_chdir] = sys_chdir,
  [SYS_unlink] = sys_unlink,
  [SYS_mmap] = sys_mmap,
  [SYS_munmap] = sys_munmap,
  [SYS_clone] = sys_clone,
  [SYS_kill] = sys_kill,
  [SYS_cv_open] = sys_cv_open,
  [SYS_cv_wait] = sys_cv_wait,
  [SYS_cv_sig] = sys_cv_sig,
  [SYS_cv_sigall] = sys_cv_sigall,
  [SYS_cv_close] = sys_cv_close,
  [SYS_pipe] = sys_pipe,
  [SYS_link] = sys_link,
  [SYS_symlink] = sys_symlink};
