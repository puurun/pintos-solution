#include <hash.h>
#include "vm/page.h"
#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <string.h>




static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

// use hash_bytes as hash function
unsigned sup_hash_func(const struct hash_elem *e, void *aux){
  const struct sup_page *sp = hash_entry(e, struct sup_page, elem);
  return hash_bytes(&sp->vaddr, sizeof sp->vaddr);
}

bool sup_hash_less_func(const struct hash_elem *a,
						const struct hash_elem *b,
						void *aux){
  const struct sup_page *as = hash_entry(a, struct sup_page, elem);
  const struct sup_page *bs = hash_entry(b, struct sup_page, elem);
  return as->vaddr < bs->vaddr;
}


void init_sup_page_table(struct thread *t){
  hash_init(&t->sup_page_table, sup_hash_func,
            sup_hash_less_func, NULL);
}


struct sup_page *sup_page_find_with_vaddr(void *vaddr){
  struct sup_page sp;
  struct thread *t = thread_current();

  vaddr = pg_round_down(vaddr);
  sp.vaddr = vaddr;

  struct hash_elem *cur_hash_elem = hash_find(&t->sup_page_table, &sp.elem);
  if (cur_hash_elem == NULL)
    return NULL;

  return hash_entry(cur_hash_elem, struct sup_page, elem);
}

void sup_page_table_stack_growth(void *vaddr){
  struct thread *t = thread_current();
  vaddr = pg_round_down(vaddr);

  while(vaddr < PHYS_BASE && sup_page_find_with_vaddr(vaddr) == NULL){
    struct sup_page *sp = malloc(sizeof(struct sup_page));
    sp->pinned = true;
    sp->type = PG_STACK;
    sp->vaddr = vaddr;
    sp->writable = true;
    sp->faddr = NULL;

    hash_insert(&t->sup_page_table, &sp->elem);
    vaddr += PGSIZE;
    sp->pinned = false;
    lock_init(&sp->page_lock);
  }
  // allocation of frame is done later in exception
}

void sup_page_table_insert_file(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable){
  struct thread *t = thread_current();
  // record information into supplementary page table
  struct sup_page *sp = malloc(sizeof(struct sup_page));
  sp->type = PG_FILE;
  sp->file = file;
  sp->ofs = ofs;
  sp->page_read_bytes = page_read_bytes;
  sp->page_zero_bytes = page_zero_bytes;
  sp->writable = writable;
  sp->pinned = false;
  sp->faddr = NULL;
  lock_init(&sp->page_lock);

  sp->vaddr = upage;
  hash_insert(&t->sup_page_table, &sp->elem);
}

void sup_page_table_insert_mmap(struct file *file, off_t ofs, uint8_t *upage,
              uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable,
              int mid){

  struct thread *t = thread_current();
  // record information into supplementary page table
  struct sup_page *sp = malloc(sizeof(struct sup_page));
  sp->type = PG_MMAP;
  sp->file = file;
  sp->ofs = ofs;
  sp->page_read_bytes = page_read_bytes;
  sp->page_zero_bytes = page_zero_bytes;
  sp->writable = writable;
  sp->mid = mid;
  sp->pinned = false;
  sp->faddr = NULL;
  lock_init(&sp->page_lock);

  sp->vaddr = upage;
  hash_insert(&t->sup_page_table, &sp->elem);
}

void load_sup_page(struct sup_page *sp){

  struct frame *frame;
  bool prev_file_lock = false;
  struct thread *t = thread_current();

  // Use supplementary page table to know what kind of page
  switch (sp->type){
    case PG_FILE: case PG_MMAP:
      lock_acquire(&frame_lock);
      // get page of memory from frame allocator
      frame = frame_table_get_frame(sp);
      
      lock_acquire(&file_lock);
      file_seek(sp->file, sp->ofs);
      // load page
      if(file_read (sp->file, frame->addr, sp->page_read_bytes)
          != (int) sp->page_read_bytes){
            frame_table_free_frame(frame);
            ASSERT(false); // TODO: think about what to do if load fail
      }

      memset(frame->addr + sp->page_read_bytes, 0, sp->page_zero_bytes);
      lock_release(&file_lock);
      
      // add page to process address space
      if (!install_page (sp->vaddr, frame->addr, sp->writable)){
        printf("file\n");
        actual_exit(-1);
      }

      sp->faddr = frame->addr;
      frame->not_evict = false;
      lock_release(&frame_lock);
      return;
    

    case PG_STACK:
      lock_acquire(&frame_lock);
      frame = frame_table_get_frame(sp);

      bool success = install_page(sp->vaddr, frame->addr, true);
      if (!success){
        printf("stk\n");
        actual_exit(-1);
      }

      sp->faddr = frame->addr;
      frame->not_evict = false;
      lock_release(&frame_lock);
      return;
    

    // get frame and write from swap
    case PG_SWAP:
      lock_acquire(&frame_lock);
      frame = frame_table_get_frame(sp);
      swap_load_from_swap(sp->swap_num, frame->addr);
      sp->type = sp->prev_type;

      if(!install_page(sp->vaddr, frame->addr, sp->writable)){
        printf("swp\n");
        actual_exit(-1);
      }
      sp->faddr = frame->addr;
      frame->not_evict = false;
      lock_release(&frame_lock);
      return;

    default:
      break;
  }

  

}




