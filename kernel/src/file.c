#include "klib.h"
#include "file.h"

#define TOTAL_FILE 128

file_t files[TOTAL_FILE];
//代表整个操作系统可以使用的file_t就这么多
//这个表为系统打开文件表，里面装着这个系统打开的所有文件
static file_t *falloc() {
  // Lab3-1: find a file whose ref==0, init it, inc ref and return it, return NULL if none
  for(int i = 0; i < TOTAL_FILE; i++)
  {
	  if(files[i].ref == 0)
	  {
		files[i].type = TYPE_NONE;
		files[i].ref++;
		return &files[i];
	  }
  }
  return NULL;
  //TODO();
}

file_t *fopen(const char *path, int mode) {
  file_t *fp = falloc();
  inode_t *ip = NULL;
  if (!fp) goto bad;
  // TODO: Lab3-2, determine type according to mode
  // iopen in Lab3-2: if file exist, open and return it
  //       if file not exist and type==TYPE_NONE, return NULL
  //       if file not exist and type!=TYPE_NONE, create the file as type
  // you can ignore this in Lab3-1
  int open_type = 114514;
  if((mode & O_CREATE) == 0)
	  open_type = TYPE_NONE;
  else if((mode & O_DIR) != 0)
	  open_type = TYPE_DIR;
  else 
	  open_type = TYPE_FILE;
  ip = iopen(path, open_type);
  if (!ip) goto bad;
  int type = itype(ip);
  if (type == TYPE_FILE || type == TYPE_DIR) {
    // TODO: Lab3-2, if type is not DIR, go bad if mode&O_DIR
	if((mode & O_DIR) && type != TYPE_DIR)
		goto bad;
    // TODO: Lab3-2, if type is DIR, go bad if mode WRITE or TRUNC
	if((type == O_DIR) && ((mode & O_WRONLY) || (mode & O_RDWR) || (mode & O_TRUNC)))
		goto bad;
    // TODO: Lab3-2, if mode&O_TRUNC, trunc the file
	if((type == TYPE_FILE) && ((mode & O_TRUNC) != 0))
		itrunc(ip);

    fp->type = TYPE_FILE; // file_t don't and needn't distingush between file and dir
    fp->inode = ip;
    fp->offset = 0;
  } else if (type == TYPE_DEV) {
    fp->type = TYPE_DEV;
    fp->dev_op = dev_get(idevid(ip));
    iclose(ip);
    ip = NULL;
  } else assert(0);
  fp->readable = !(mode & O_WRONLY);
  fp->writable = (mode & O_WRONLY) || (mode & O_RDWR);
  return fp;
bad:
  if (fp) fclose(fp);
  if (ip) iclose(ip);
  return NULL;
}

int fread(file_t *file, void *buf, uint32_t size) {
  // Lab3-1, distribute read operation by file's type
  // remember to add offset if type is FILE (check if iread return value >= 0!)
  if (!file->readable) return -1;
  int ret;
  if(file->type == TYPE_FILE)
  {
	  //磁盘文件（包括普通文件or文件夹）
	  ret = iread(file->inode, file->offset, buf, size);
	  if(ret == -1)
		  return -1;
	  file->offset += ret;
	  return ret;
  }
  else if(file->type == TYPE_DEV)
  {
	  //设备文件
	  ret = file->dev_op->read(buf, size);
	  return ret;
  }
  else
	  assert(0);
  //TODO();
}

int fwrite(file_t *file, const void *buf, uint32_t size) {
  // Lab3-1, distribute write operation by file's type
  // remember to add offset if type is FILE (check if iwrite return value >= 0!)
  if (!file->writable) return -1;
  int ret;
  if(file->type == TYPE_FILE)
  {
	ret = iwrite(file->inode, file->offset, buf, size);
	if(ret == -1)
		return -1;
	file->offset += ret;
    return ret;	
  }
  else if(file->type == TYPE_DEV)
  {
	  ret = file->dev_op->write(buf, size);
	  return ret;
  }
  else
	  assert(0);
  //TODO();
}

uint32_t fseek(file_t *file, uint32_t off, int whence) {
  // Lab3-1, change file's offset, do not let it cross file's size
  if (file->type == TYPE_FILE) {
    //TODO();
	uint32_t sz = isize(file->inode);
	switch(whence)
	{
		case SEEK_SET:
			//文件开头向后off处
			file->offset = (off < sz) ? off : sz;
			break;
		case SEEK_CUR:
			//当前偏移量向后off
			file->offset = (file->offset + off < sz) ? file->offset + off : sz;
			break;
		case SEEK_END:
			//结尾向后off处
			file->offset = sz + off;
			if(file->offset < 0)
				file->offset = 0;
			else if(file->offset >= sz)
				file->offset = sz;
			//off可以为负
			break;
		default:
			assert(0);
	}
	/*
	if(file->offset >= sz)
		return -1;
	*/
	//这一段不能写，因为超过了（即不合法）就需要它自行调整，而不是输出-1
	return file->offset;
  }
  return -1;
}

file_t *fdup(file_t *file) {
  // Lab3-1, inc file's ref, then return itself
  //用于fork时复制进程所用文件及dup系统调用
  file->ref++;
  return file;
  //TODO();
}

void fclose(file_t *file) {
  // Lab3-1, dec file's ref, if ref==0 and it's a file, call iclose
  //代表该进程关闭此文件
  file->ref--;
  if(file->ref == 0 && file->type == TYPE_FILE)
	  iclose(file->inode);
  //TODO();
}
