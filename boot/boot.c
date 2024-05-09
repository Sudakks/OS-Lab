#include "boot.h"

// DO NOT DEFINE ANY NON-LOCAL VARIBLE!

void load_kernel() {
/*
  char hello[] = {'\n', 'h', 'e', 'l', 'l', 'o', '\n', 0};
  putstr(hello);
  while (1) ;
*/
  // remove both lines above before write codes below
  Elf32_Ehdr *elf = (void *)0x8000;
  copy_from_disk(elf, 255 * SECTSIZE, SECTSIZE);
  Elf32_Phdr *ph, *eph;
  ph = (void*)((uint32_t)elf + elf->e_phoff);
  /*ph是程序头表所在的位置*/
  eph = ph + elf->e_phnum;
  for (; ph < eph; ph++) {
    if (ph->p_type == PT_LOAD) {
       char* dst = (char*)ph->p_vaddr;
       char* src = (char*)(0x8000 + ph->p_offset); 
       memcpy(dst, src, ph->p_filesz);
       dst = (char*)(ph->p_vaddr + ph->p_filesz);
       memset(dst, 0, ph->p_memsz - ph->p_filesz);
      // TODO: Lab1-2, Load kernel and jump
    }
  }
  uint32_t entry = elf->e_entry; // change me
  ((void(*)())entry)();
}
