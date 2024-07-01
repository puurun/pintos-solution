#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/thread.h"
#include <stdbool.h>


enum page_type {PG_FILE, PG_MMAP, PG_STACK, PG_SWAP};
// supplementary page table
struct sup_page {
    enum page_type type;
    enum page_type prev_type;

    // page related to files
    struct file *file;
    off_t ofs;
    size_t page_read_bytes;
    size_t page_zero_bytes;
    bool writable;
    int mid; // used if mmap was used to map this file to vm

    int swap_num;
    struct lock page_lock;
    bool pinned;
    // page related to stack

    void *vaddr; // address of current page 
    void *faddr; // address of frame
    struct hash_elem elem;
};

// structure for mmap list. need len, file, and start_addr
struct mmap_file {
  struct file *file;
  void *start_addr;
  off_t len;
  int mid;
  struct list_elem elem;
};


void load_sup_page(struct sup_page *sp);
void init_sup_page_table(struct thread *);
void sup_page_table_stack_growth(void *vaddr);
struct sup_page *sup_page_find_with_vaddr(void *vaddr);
void sup_page_table_insert_file(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable);
void sup_page_table_insert_mmap(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable,
              int mid);

#endif
