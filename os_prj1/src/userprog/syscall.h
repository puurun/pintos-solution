#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"
#include "threads/interrupt.h"
#include "userprog/process.h"

void syscall_init (void);

bool check_valid_pointer(uint32_t *pd, void *uaddr);
void sys_halt(void);
void sys_exit(struct thread *t, struct intr_frame *f);
void sys_exec(struct thread *t, struct intr_frame *f);
void sys_wait(struct thread *t, struct intr_frame *f);
void sys_write(struct thread *t, struct intr_frame *f);
void sys_read(struct thread *t, struct intr_frame *f);
void sys_fibonacci(struct thread *t, struct intr_frame *f);
void sys_max_of_four_int(struct thread *t, struct intr_frame *f);
void actual_exit(int status);
void read_stack_int32(uint32_t *pd, void *esp, int *dest);
void read_stack_pointer(uint32_t *pd, void *esp, void **dest);
void read_stack_uint32(uint32_t *pd, void *esp, unsigned *dest);

#endif /* userprog/syscall.h */
