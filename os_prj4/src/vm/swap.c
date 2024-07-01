#include "vm/swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include <stdbool.h>
#include <stdio.h>
#include <bitmap.h>


struct block *swap_block;
int swap_size;
struct bitmap *swap_used;
struct lock swap_lock;


void swap_init(){
  swap_block = block_get_role(BLOCK_SWAP);

  // determines how many pages you can swap
  // number of sectors * sector size / pgsize
  swap_size = block_size(swap_block) * BLOCK_SECTOR_SIZE / PGSIZE;
  swap_used = bitmap_create(swap_size);
  bitmap_set_all(swap_used, false);

  lock_init(&swap_lock);
}

// saves page in addr to swap
// returns swap number
int swap_save_into_swap(void *addr){
  int unused_swap = -1;

  lock_acquire(&swap_lock);
  // find unused swap space
  for(int i=0; i<swap_size; i++){
    if(bitmap_test(swap_used, i) == false){
      unused_swap = i;
      break;
    }
  }

  // couldn't find unused swap
  if (unused_swap == -1){
    return -1;
  }

  bitmap_set(swap_used, unused_swap, true);

  // loops page and writes block sectors
  int cur_sector = 0;
  int swap_sec = unused_swap * (PGSIZE / BLOCK_SECTOR_SIZE);
  for(int i=0; i<PGSIZE/BLOCK_SECTOR_SIZE; i++){
    block_write(swap_block, swap_sec+cur_sector, addr);

    addr += BLOCK_SECTOR_SIZE;
    cur_sector++;
  }

  lock_release(&swap_lock);

  return unused_swap;
}


// uses swap number
// frees swap space, copies from swap to addr
void swap_load_from_swap(int swap_number, void *addr){
  if(bitmap_test(swap_used, swap_number) == false)
    return;

  lock_acquire(&swap_lock);

  bitmap_set(swap_used, swap_number, false);
  int cur_sector = 0;
  int swap_sec = swap_number * (PGSIZE / BLOCK_SECTOR_SIZE);
  for(int i=0; i<PGSIZE/BLOCK_SECTOR_SIZE; i++){
    block_read(swap_block, swap_sec+cur_sector, addr);

    addr += BLOCK_SECTOR_SIZE;
    cur_sector++;
  }

  lock_release(&swap_lock);

}
