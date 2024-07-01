#include <hash.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <stdio.h>



void frame_table_evict_frame(void);
// manages frames for user
struct hash frame_table;
struct list lru_list;
struct list_elem* cur_lru_elem;
struct lock frame_lock;

// returns a frame requested by user 
struct frame* frame_table_get_frame(struct sup_page *sp){
  struct thread *t = thread_current();

  void *phys = palloc_get_page(PAL_USER);
  // failed to get page from user pool
  // currently, just assert
  while (phys == NULL){
    // find access bit = 0
    frame_table_evict_frame();
    phys = palloc_get_page(PAL_USER);
  }
  
  
  // has to free later
  struct frame *f = malloc(sizeof(struct frame));
  f->addr = phys; // allocated by palloc
  f->pd = t->pagedir;
  f->vaddr = sp->vaddr;
  f->sp = sp;
  f->not_evict = true;
  f->has = true;
  
  // store frame in frame table

  hash_insert(&frame_table, &f->elem);
  list_push_back(&lru_list, &f->lru_elem);


  return f;
}


// deletes frame from hash table
// frees dynamically allocated things
void frame_table_free_frame(struct frame *f){

  f->has = false;
  f->sp->faddr = NULL;
  f->vaddr = NULL;
  palloc_free_page(f->addr);
  hash_delete(&frame_table, &f->elem);
  list_remove(&f->lru_elem);
  free(f);

}

void frame_table_evict_frame(){

  if(cur_lru_elem == NULL){
    cur_lru_elem = list_begin(&lru_list);
  }

  struct frame *victim_frame;
  while(true){
    struct frame *f = list_entry(cur_lru_elem, struct frame, lru_elem);
    if(f->not_evict){
      cur_lru_elem = list_next(cur_lru_elem);
      if(cur_lru_elem == list_end(&lru_list)){
        cur_lru_elem = list_begin(&lru_list);
      }
      continue;
    }
    if(pagedir_is_accessed(f->pd, f->vaddr)){
      pagedir_set_accessed(f->pd, f->vaddr, false); 
    }
    else{
      pagedir_clear_page(f->pd, f->vaddr);
      victim_frame = f;
      cur_lru_elem = list_next(cur_lru_elem);
      if(cur_lru_elem == list_end(&lru_list)){
        cur_lru_elem = list_begin(&lru_list);
      }
      break;
    }

    cur_lru_elem = list_next(cur_lru_elem);
    if(cur_lru_elem == list_end(&lru_list)){
      cur_lru_elem = list_begin(&lru_list);
    }
  }
  
  // got victim frame
  // determine vaddr type, write to disk if needed
  struct frame *f = victim_frame;


  struct sup_page *sp = f->sp;
  switch(sp->type){
    case PG_MMAP:
      if(pagedir_is_dirty(f->pd, sp->vaddr)){
        // write dirty file to swap
        sp->prev_type = sp->type;
        sp->type = PG_SWAP;
        sp->swap_num = swap_save_into_swap(f->addr);
        sp->faddr = NULL;
      }

      // not dirty -> just free

      break;

    case PG_FILE: case PG_STACK:
      sp->prev_type = sp->type;
      sp->type = PG_SWAP;
      sp->swap_num = swap_save_into_swap(f->addr);
      sp->faddr = NULL;
      break;
    case PG_SWAP:
      break;
  }


  frame_table_free_frame(f);

}

struct frame *frame_table_find_with_addr(void *addr){
  struct frame f;
  struct thread *t = thread_current();

  addr = pg_round_down(addr);
  f.addr = addr;

  struct hash_elem *cur_hash_elem = hash_find(&frame_table, &f.elem);
  if (cur_hash_elem == NULL)
    return NULL;
  struct frame *ff = hash_entry(cur_hash_elem, struct frame, elem);

  return ff;
}




// use hash_bytes as hash function. hash 4 bytes (address)
unsigned simple_hash_func(const struct hash_elem *e, void *aux){
  const struct frame *f = hash_entry(e, struct frame, elem);
  return hash_bytes(&f->addr, sizeof(f->addr));
}

bool simple_hash_less_func(const struct hash_elem *a,
							   const struct hash_elem *b,
							   void *aux){
  const struct frame *af = hash_entry(a, struct frame, elem);
  const struct frame *bf = hash_entry(b, struct frame, elem);
  return af->addr < bf->addr;
}

void frame_table_init(){
  hash_init(&frame_table, simple_hash_func, 
	  simple_hash_less_func, NULL); 

  list_init(&lru_list);
  lock_init(&frame_lock);
  cur_lru_elem = NULL;
  
}








