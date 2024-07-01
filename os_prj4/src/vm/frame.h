#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include <stdbool.h>


struct frame {
  struct hash_elem elem;
  // addr of frame
  void *addr;
  void *vaddr; // virtual address the frame is using
  uint32_t *pd;
  struct sup_page *sp;
  bool not_evict;
  bool has;

  struct list_elem lru_elem;

  // additional info on frame
  // for eviction
};

extern struct lock frame_lock;
struct frame *frame_table_get_frame(struct sup_page*);
void frame_table_free_frame(struct frame*);
void frame_table_init(void);
struct frame *frame_table_find_with_addr(void *addr);



#endif
