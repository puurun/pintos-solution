#ifndef VM_SWAP_H
#define VM_SWAP_H


void swap_init(void);
int swap_save_into_swap(void *addr);
void swap_load_from_swap(int swap_number, void *addr);

#endif
