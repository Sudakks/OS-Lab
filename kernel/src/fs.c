#include "klib.h"
#include "fs.h"
#include "disk.h"
#include "proc.h"

#ifdef EASY_FS

#define MAX_FILE  (SECTSIZE / sizeof(dinode_t))
#define MAX_DEV   16
#define MAX_INODE (MAX_FILE + MAX_DEV)

// On disk inode
typedef struct dinode {
  uint32_t start_sect;
  uint32_t length;
  char name[MAX_NAME + 1];
} dinode_t;

// On OS inode, dinode with special info
struct inode {
  int valid;
  int type;
  int dev; // dev_id if type==TYPE_DEV
  dinode_t dinode;
};

static inode_t inodes[MAX_INODE];
//表示整个OS我们可以打开的inode，即活动inode表

void init_fs() {
  dinode_t buf[MAX_FILE];
  read_disk(buf, 256);
  for (int i = 0; i < MAX_FILE; ++i) {
    inodes[i].valid = 1;
    inodes[i].type = TYPE_FILE;
    inodes[i].dinode = buf[i];
  }
}

inode_t *iopen(const char *path, int type) {
  for (int i = 0; i < MAX_INODE; ++i) {
    if (!inodes[i].valid) continue;
    if (strcmp(path, inodes[i].dinode.name) == 0) {
      return &inodes[i];
    }
  }
  return NULL;
}

int iread(inode_t *inode, uint32_t off, void *buf, uint32_t len) {
  assert(inode);
  char *cbuf = buf;
  char dbuf[SECTSIZE];
  uint32_t curr = -1;
  uint32_t total_len = inode->dinode.length;
  uint32_t st_sect = inode->dinode.start_sect;
  int i;
  for (i = 0; i < len && off < total_len; ++i, ++off) {
    if (curr != off / SECTSIZE) {
      read_disk(dbuf, st_sect + off / SECTSIZE);
      curr = off / SECTSIZE;
    }
    *cbuf++ = dbuf[off % SECTSIZE];
  }
  return i;
}

void iadddev(const char *name, int id) {
  assert(id < MAX_DEV);
  inode_t *inode = &inodes[MAX_FILE + id];
  inode->valid = 1;
  inode->type = TYPE_DEV;
  inode->dev = id;
  strcpy(inode->dinode.name, name);
}

uint32_t isize(inode_t *inode) {
  return inode->dinode.length;
}

int itype(inode_t *inode) {
  return inode->type;
}

uint32_t ino(inode_t *inode) {
  return inode - inodes;
}

int idevid(inode_t *inode) {
  return inode->type == TYPE_DEV ? inode->dev : -1;
}

int iwrite(inode_t *inode, uint32_t off, const void *buf, uint32_t len) {
  panic("write doesn't support");
}

void itrunc(inode_t *inode) {
  panic("trunc doesn't support");
}

inode_t *idup(inode_t *inode) {
  return inode;
}

void iclose(inode_t *inode) { /* do nothing */ }

int iremove(const char *path) {
  panic("remove doesn't support");
}

#else

#define DISK_SIZE (128 * 1024 * 1024)
#define BLK_NUM   (DISK_SIZE / BLK_SIZE)

#define NDIRECT   12
#define NINDIRECT (BLK_SIZE / sizeof(uint32_t))

#define IPERBLK   (BLK_SIZE / sizeof(dinode_t)) // inode num per blk

// super block
typedef struct super_block {
  uint32_t bitmap; // block num of bitmap
  uint32_t istart; // start block no of inode blocks
  uint32_t inum;   // total inode num
  uint32_t root;   // inode no of root dir
} sb_t;

// On disk inode
typedef struct dinode {
  uint32_t type;   // file type
  uint32_t device; // if it is a dev, its dev_id
  uint32_t size;   // file size
  uint32_t addrs[NDIRECT + 1]; // data block addresses, 12 direct and 1 indirect
} dinode_t;

struct inode {
  int no;
  int ref;
  int del;
  dinode_t dinode;
};

#define SUPER_BLOCK 32
static sb_t sb;

void init_fs() {
  bread(&sb, sizeof(sb), SUPER_BLOCK, 0);
}

#define I2BLKNO(no)  (sb.istart + no / IPERBLK)
#define I2BLKOFF(no) ((no % IPERBLK) * sizeof(dinode_t))

static void diread(dinode_t *di, uint32_t no) {
  bread(di, sizeof(dinode_t), I2BLKNO(no), I2BLKOFF(no));
}

static void diwrite(const dinode_t *di, uint32_t no) {
  bwrite(di, sizeof(dinode_t), I2BLKNO(no), I2BLKOFF(no));
}

static uint32_t dialloc(int type) {
  // Lab3-2: iterate all dinode, find a empty one (type==TYPE_NONE)
  // set type, clean other infos and return its no (remember to write back)
  // if no empty one, just abort
  // note that first (0th) inode always unused, because dirent's inode 0 mark invalid
  dinode_t dinode;
  for (uint32_t i = 1; i < sb.inum; ++i) {
    diread(&dinode, i);
	if(dinode.type == TYPE_NONE)
	{
		//找到空闲的了
		dinode.type = type;
		diwrite(&dinode, i);
		return i;
	}
    //TODO();
  }
  assert(0);
}

static void difree(uint32_t no) {
  dinode_t dinode;
  memset(&dinode, 0, sizeof dinode);
  diwrite(&dinode, no);
}

static uint32_t balloc() {
  // Lab3-2: iterate bitmap, find one free block
  // set the bit, clean the blk (can call bzero) and return its no
  // if no free block, just abort
  uint32_t byte;
  for (int i = 0; i < BLK_NUM / 32; ++i) {
    bread(&byte, 4, sb.bitmap, i * 4);
    if (byte != 0xffffffff) {
		//说明有一个0的位，找出其中一个为0的比特
		for(int j = 0; j < 32; j++)
		{
			if((byte & (1 << j)) == 0)
			{
				//说明32*i+j号逻辑块是空闲的
				bzero(32 * i + j);
				byte |= (1 << j);	
				bwrite(&byte, 4, sb.bitmap, i * 4);
				return 32 * i + j;
			}
		}
      //TODO();
    }
  }
  assert(0);
}

static void bfree(uint32_t blkno) {
  // Lab3-2: clean the bit of blkno in bitmap
  assert(blkno >= 64); // cannot free first 64 block
  uint8_t bitmap_no;
  bread(&bitmap_no, 1, sb.bitmap, blkno / 8);
  bitmap_no &= ~(1 << (blkno % 8));
  bwrite(&bitmap_no, 1, sb.bitmap, blkno / 8);
  //TODO();
}

#define INODE_NUM 128
static inode_t inodes[INODE_NUM];

static inode_t *iget(uint32_t no) {
  // Lab3-2
  // if there exist one inode whose no is just no, inc its ref and return it
  // otherwise, find a empty inode slot, init it and return it
  // if no empty inode slot, just abort
  // 在活动inode表中打开编号为no的inode
  for(int i = 0; i < INODE_NUM; i++)
  {
	  if(inodes[i].no == no)
	  {
		  inodes[i].ref++;
		  return &inodes[i];
	  }
  }
  //此时表示这个编号的inode没有打开
  for(int i = 0; i < INODE_NUM; i++)
  {
	  if(inodes[i].ref == 0)
	  {
		  //init
		  inodes[i].no = no;
		  inodes[i].ref = 1;
		  inodes[i].del = 0;
		  diread(&inodes[i].dinode, no);
		  return &inodes[i];
	  }
  }
  assert(0);
  //TODO();
}

static void iupdate(inode_t *inode) {
  // Lab3-2: sync the inode->dinode to disk
  // call me EVERYTIME after you edit inode->dinode
  diwrite(&inode->dinode, inode->no);
}


// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return NULL.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = NULL
//
static const char* skipelem(const char *path, char *name) {
  const char *s;
  int len;
  while (*path == '/') path++;
  if (*path == 0) return 0;
  s = path;
  while(*path != '/' && *path != 0) path++;
  len = path - s;
  if (len >= MAX_NAME) {
    memcpy(name, s, MAX_NAME);
    name[MAX_NAME] = 0;
  } else {
    memcpy(name, s, len);
    name[len] = 0;
  }
  while (*path == '/') path++;
  return path;
}

static void idirinit(inode_t *inode, inode_t *parent) {
  // Lab3-2: init the dir inode, i.e. create . and .. dirent
  // 给inode对应的文件（一定是目录创建.和..）
  assert(inode->dinode.type == TYPE_DIR);
  assert(parent->dinode.type == TYPE_DIR); // both should be dir
  assert(inode->dinode.size == 0); // inode shoule be empty
  dirent_t dirent;
  // set .
  // 这一级
  dirent.inode = inode->no;
  strcpy(dirent.name, ".");
  iwrite(inode, 0, &dirent, sizeof dirent);
  // set ..
  // 上一级
  dirent.inode = parent->no;
  strcpy(dirent.name, "..");
  iwrite(inode, sizeof dirent, &dirent, sizeof dirent);
}

static inode_t *ilookup(inode_t *parent, const char *name, uint32_t *off, int type) {
  // Lab3-2: iterate the parent dir, find a file whose name is name
  // if off is not NULL, store the offset of the dirent_t to it
  // if no such file and type == TYPE_NONE, return NULL
  // if no such file and type != TYPE_NONE, create the file with the type
  assert(parent->dinode.type == TYPE_DIR); // parent must be a dir
  dirent_t dirent;
  uint32_t size = parent->dinode.size, empty = size;
  for (uint32_t i = 0; i < size; i += sizeof dirent) {
    // directory is a file containing a sequence of dirent structures
    iread(parent, i, &dirent, sizeof dirent);
    if (dirent.inode == 0) {
      // a invalid dirent, record the offset (used in create file), then skip
      if (empty == size) empty = i;
      continue;
    }
    // a valid dirent, compare the name
	if(strcmp(dirent.name, name) == 0)
	{
		//找到了名字为name的文件
		if(off != NULL)
			*off = i;
		return iget(dirent.inode);
	}
    //TODO();
  }
  // not found
  if (type == TYPE_NONE) return NULL;
  // need to create the file, first alloc inode, then init dirent, write it to parent
  // if you create a dir, remember to init it's . and ..
  uint32_t alloc_no = dialloc(type);
  inode_t *open_inode = iget(alloc_no);
  if(type == TYPE_DIR)
	  idirinit(open_inode, parent);
  dirent_t new_dirent;//在内存中初始化一个内存项？
  new_dirent.inode = alloc_no;
  strcpy(new_dirent.name, name);
  assert(strcmp(new_dirent.name, name) == 0);
  iwrite(parent, empty, &new_dirent, sizeof(dirent_t));
  if(off != NULL)
	  *off = empty;
  return open_inode;
  //TODO();
}

static inode_t *iopen_parent(const char *path, char *name) {
  // Lab3-2: open the parent dir of path, store the basename to name
  // if no such parent, return NULL
  inode_t *ip, *next;
  // set search starting inode
  if (path[0] == '/') {
    ip = iget(sb.root);
  } else {
    ip = idup(proc_curr()->cwd);
  }
  assert(ip);
  while ((path = skipelem(path, name))) {
    // curr round: need to search name in ip
    if (ip->dinode.type != TYPE_DIR) {
      // not dir, cannot search
      iclose(ip);
      return NULL;
    }
    if (*path == 0) {
      // last search, return ip because we just need parent
      return ip;
    }
    // not last search, need to continue to find parent
    next = ilookup(ip, name, NULL, 0);
    if (next == NULL) {
      // name not exist
      iclose(ip);
      return NULL;
    }
    iclose(ip);
    ip = next;
  }
  iclose(ip);
  return NULL;
}

inode_t *iopen(const char *path, int type) {
  // Lab3-2: if file exist, open and return it
  // if file not exist and type==TYPE_NONE, return NULL
  // if file not exist and type!=TYPE_NONE, create the file as type
  char name[MAX_NAME + 1];
  if (skipelem(path, name) == NULL) {
    // no parent dir for path, path is "" or "/"
    // "" is an invalid path, "/" is root dir
    return path[0] == '/' ? iget(sb.root) : NULL;
  }
  // path do have parent, use iopen_parent and ilookup to open it
  // remember to close the parent inode after you ilookup it
  inode_t *iopen_par = iopen_parent(path, name); 
    if(iopen_par == NULL)
	  return NULL;
  inode_t *cur_inode = ilookup(iopen_par, name, NULL, type);
  iclose(iopen_par);
  return cur_inode;
  //TODO();
}

static uint32_t iwalk(inode_t *inode, uint32_t no) {
  // return the blkno of the file's data's no th block, if no, alloc it
  // 返回inode对应的文件数据使用的第no个逻辑块编号，不存在就申请
  if (no < NDIRECT) {
    // direct address
	uint32_t addr_blk = inode->dinode.addrs[no];
	if(addr_blk == 0)
	{
		addr_blk = balloc();
		inode->dinode.addrs[no] = addr_blk;
		iupdate(inode);
	}
	return addr_blk;
    //TODO();
  }
  no -= NDIRECT;
  if (no < NINDIRECT) {
    // indirect address
	uint32_t indirect_addr_blk = inode->dinode.addrs[NDIRECT];
	if(indirect_addr_blk == 0)
	{
		indirect_addr_blk = balloc();
		inode->dinode.addrs[NDIRECT] = indirect_addr_blk;
		iupdate(inode);
	}
	uint32_t curr_blk;
	bread(&curr_blk, 4, indirect_addr_blk, 4 * no);
	if(curr_blk == 0)
	{
		curr_blk = balloc();
		bwrite(&curr_blk, 4, indirect_addr_blk, 4 * no);
	}
	return curr_blk;
    //TODO();
  }
  assert(0); // file too big, not need to handle this case
}

int iread(inode_t *inode, uint32_t off, void *buf, uint32_t len) {
  // Lab3-2: read the inode's data [off, MIN(off+len, size)) to buf
  // use iwalk to get the blkno and read blk by blk
  // 第i个字节在这个文件所用的第i/4096个逻辑块的第i%4096个字节
  uint32_t sz = inode->dinode.size;
  uint32_t max_len = (len < sz - off) ? len : sz - off; 
  uint32_t blk_off, blk_no, can_read_len;
  uint32_t have_read;
  uint32_t tmp1, tmp2;
  //if(max_len < 0)
	//  return -1;
  for(have_read = 0; have_read < max_len;)
  {
	blk_off = off / BLK_SIZE;
	//计算出现在要读的是这个文件数据的第几个逻辑块
	blk_no = iwalk(inode, blk_off);
	//计算得到这个逻辑块的编号
	tmp1 = BLK_SIZE - (off % BLK_SIZE);
	tmp2 = max_len - have_read;
    can_read_len = (tmp1 < tmp2) ? tmp1 : tmp2;	
	bread(buf, can_read_len, blk_no, off % BLK_SIZE);
	buf += can_read_len;
	off += can_read_len;
	have_read += can_read_len;
  }
  return have_read;
  //TODO();
}

int iwrite(inode_t *inode, uint32_t off, const void *buf, uint32_t len) {
  // Lab3-2: write buf to the inode's data [off, off+len)
  // if off>size, return -1 (can not cross size before write)
  // if off+len>size, update it as new size (but can cross size after write)
  // use iwalk to get the blkno and read blk by blk
  uint32_t sz = inode->dinode.size;
  if(off > sz)
	  return -1;//不能一开始就大于
  if(off + len > sz)
  {
	  //一开始就判断，如果写的时候会大于，那么更新文件大小
	  inode->dinode.size = off + len;
	  iupdate(inode);
  }
  uint32_t tmp1, tmp2;
  uint32_t blk_off, blk_no, can_write_len;
  uint32_t have_write;
  for(have_write = 0; have_write < len;)
  {
	blk_off = off / BLK_SIZE;
	blk_no = iwalk(inode, blk_off);
	tmp1 = BLK_SIZE - (off % BLK_SIZE);
	tmp2 = len - have_write;
	can_write_len = (tmp1 < tmp2) ? tmp1 : tmp2; 
	//if(can_write_len + off % BLK_SIZE >= BLK_SIZE)
		//printf("len = %d, off = %d\n", can_write_len, off);
	//assert(can_write_len + off % BLK_SIZE < BLK_SIZE);
	bwrite(buf, can_write_len, blk_no, off % BLK_SIZE);
	off += can_write_len;
	buf += can_write_len;
	have_write += can_write_len;
  }
  assert(have_write == len);
  return have_write;
  //TODO();
}

void itrunc(inode_t *inode) {
  // Lab3-2: free all data block used by inode (direct and indirect)
  // mark all address of inode 0 and mark its size 0
  for(int i = 0; i < NDIRECT; i++)
  {
	  //清空直接索引块
	  if(inode->dinode.addrs[i] != 0)
	    bfree(inode->dinode.addrs[i]);
  }
  uint32_t ndirect_blk = inode->dinode.addrs[NDIRECT];
  if(ndirect_blk != 0)
  {
	  //清空间接索引
	  uint32_t blk_no;
	  for(int i = 0; i < 1024; i++)
	  {
		blk_no = iwalk(inode, i + 12);
		bfree(blk_no);
	  }
  }
  for(int i = 0; i < NDIRECT + 1; i++)
	  inode->dinode.addrs[i] = 0;
  inode->dinode.size = 0;
  iupdate(inode);
  //TODO();
}

inode_t *idup(inode_t *inode) {
  assert(inode);
  inode->ref += 1;
  return inode;
}

void iclose(inode_t *inode) {
  assert(inode);
  if (inode->ref == 1 && inode->del) {
    itrunc(inode);
    difree(inode->no);
  }
  inode->ref -= 1;
}

uint32_t isize(inode_t *inode) {
  return inode->dinode.size;
}

int itype(inode_t *inode) {
  return inode->dinode.type;
}

uint32_t ino(inode_t *inode) {
  return inode->no;
}

int idevid(inode_t *inode) {
  return itype(inode) == TYPE_DEV ? inode->dinode.device : -1;
}

void iadddev(const char *name, int id) {
  inode_t *ip = iopen(name, TYPE_DEV);
  assert(ip);
  ip->dinode.device = id;
  iupdate(ip);
  iclose(ip);
}

static int idirempty(inode_t *inode) {
  // Lab3-2: return whether the dir of inode is empty
  // the first two dirent of dir must be . and ..
  // you just need to check whether other dirent are all invalid
  assert(inode->dinode.type == TYPE_DIR);
  bool ret = 1;
  dirent_t dirent;
  uint32_t size = isize(inode);
  //因为前2个分别为.和..，所以只要看剩下的目录项
  //剩下的目录项 均为 无效目录项，才是empty
  for (uint32_t i = 2 * (sizeof dirent); i < size; i += sizeof dirent) {
    iread(inode, i, &dirent, sizeof dirent);
    if (dirent.inode != 0) 
	{
		ret = 0;
		break;
    }
  }
  return ret;
  //是空返回1
  //TODO();
}

int iremove(const char *path) {
  // Lab3-2: remove the file, return 0 on success, otherwise -1
  // first open its parent, if no parent, return -1
  // then find file in parent, if not exist, return -1
  // if the file need to remove is a dir, only remove it when it's empty
  // . and .. cannot be remove, so check name set by iopen_parent
  // remove a file just need to clean the dirent points to it and set its inode's del
  // the real remove will be done at iclose, after everyone close it
  char name[MAX_NAME + 1];
  inode_t *iopen_par = iopen_parent(path, name);
  //得到所在目录和文件名
  if(iopen_par == NULL)
	  return -1;
  char root1[5], root2[5];
  strcpy(root1, ".");
  strcpy(root2, "..");
  if(strcmp(root1, name) == 0 || strcmp(root2, name) == 0)
  {
	iclose(iopen_par);
	return -1;
  }
  uint32_t off;
  inode_t *cur_inode = ilookup(iopen_par, name, &off, TYPE_NONE);
  if(cur_inode == NULL)
  {
	  iclose(iopen_par);
	  return -1;
  }
  if(itype(cur_inode) == TYPE_DIR && idirempty(cur_inode) == 0)
  {
	  //文件目录非空
	  iclose(iopen_par);
	  iclose(cur_inode);
	  return -1;
  }
  cur_inode->del = 1;
  dirent_t dirent;
  memset(&dirent, 0, sizeof(dirent_t));
  iwrite(iopen_par, off, &dirent, sizeof(dirent_t));
  return 0;
  //TODO();
}

#endif
