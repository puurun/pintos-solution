#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int syscall_num;
  struct thread *t = thread_current();
  
  check_valid_pointer(t->pagedir, f->esp);
  read_stack_int32(t->pagedir, f->esp, &syscall_num);

  switch(syscall_num){
    case SYS_HALT:
      sys_halt();
      break;
    case SYS_EXIT:
      sys_exit(t, f);
      break;
    case SYS_EXEC:
      sys_exec(t, f);
      break;
    case SYS_WAIT:
      sys_wait(t, f);
      break;
    case SYS_WRITE:
      sys_write(t, f);
      break;
    case SYS_READ:
      sys_read(t, f);
      break;
    case SYS_FIBONACCI:
      sys_fibonacci(t, f);
      break;
    case SYS_MAX_OF_FOUR_INT:
      sys_max_of_four_int(t, f);
      break;
  }
}

bool check_valid_pointer(uint32_t *pd, void *uaddr){
    if(uaddr != NULL && is_user_vaddr(uaddr) && pagedir_get_page(pd, uaddr) != NULL){
        return true;
    }
    
    actual_exit(-1);

    // doesn't actually reach here
    return false;
}

void read_stack_int32(uint32_t *pd, void *esp, int *dest){
    check_valid_pointer(pd, esp);
    *dest = *(int*)(esp);
}

void read_stack_uint32(uint32_t *pd, void *esp, unsigned *dest){
    check_valid_pointer(pd, esp);
    *dest = *(unsigned*)(esp);
}

void read_stack_pointer(uint32_t *pd, void *esp, void **dest){
    check_valid_pointer(pd, esp);
    *dest = *(void**)(esp);
    check_valid_pointer(pd, *dest);
}


void sys_halt(){
    shutdown_power_off();
}

void actual_exit(int status){
    struct thread *t = thread_current();

    t->thread_item.exit_status = status; // sets exit status
    printf("%s: exit(%d)\n", t->name, status);
    sema_up(&t->thread_item.called_exit);
    sema_down(&t->thread_item.can_free_resources);
    thread_exit();
}
void sys_exit(struct thread *t, struct intr_frame *f){
    int status;
    read_stack_int32(t->pagedir, f->esp+4, &status);
    actual_exit(status);
}

struct thread *find_child_thread(tid_t tid){
    struct thread *t = thread_current();
    
    for(struct list_elem *e = list_begin(&t->child_process_list);
        e != list_end(&t->child_process_list);
        e = list_next(e)){

        struct list_item_thread *item = list_entry(e, struct list_item_thread, elem);
        struct thread *child_thread = item->t;

        if(tid == child_thread->tid){
            return child_thread;
        }
    }

    return NULL;

}
void sys_exec(struct thread *t, struct intr_frame *f){
    char *file;
    read_stack_pointer(t->pagedir, f->esp+4, (void**)&file);
    
    tid_t tid = process_execute(file);
    
    struct thread *child_t = find_child_thread(tid);

    sema_down(&child_t->thread_item.load_done);

    if(!child_t->thread_item.load_success){
        f->eax = -1;
        sema_up(&child_t->thread_item.can_free_resources);
    }
    else{
        f->eax = tid;
    }
}

void sys_wait(struct thread *t, struct intr_frame *f){
    int pid;
    read_stack_int32(t->pagedir, f->esp+4, &pid);
    f->eax = process_wait(pid);
}

void sys_write(struct thread *t, struct intr_frame *f){
    int fd;
    char *buffer;
    unsigned size;

    read_stack_int32(t->pagedir, f->esp+4, &fd);
    read_stack_pointer(t->pagedir, f->esp+8, (void**)&buffer);
    read_stack_uint32(t->pagedir, f->esp+12, &size);

    if(fd == 1){
        putbuf(buffer, size);
        f->eax = size; 
    }
    else{
        f->eax = -1;
    }
}

void sys_read(struct thread *t, struct intr_frame *f){
    int fd;
    char *buffer;
    unsigned int size;
    read_stack_int32(t->pagedir, f->esp+4, &fd);
    read_stack_pointer(t->pagedir, f->esp+8, (void**)&buffer);
    read_stack_uint32(t->pagedir, f->esp+12, &size);

    if(fd == 0){
        for(unsigned i=0; i<size; i++){
            char c = input_getc();
            buffer[i] = c;
        }
        f->eax = size;
    }
    else {
        f->eax = -1;
    }
}

void sys_fibonacci(struct thread *t, struct intr_frame *f){
    int n;
    read_stack_int32(t->pagedir, f->esp+4, &n);
    int a0 = 0;
    int a1 = 1;
    int a2 = 1;
    if(n==0){
        f->eax = a0;
    }
    else if (n==1){
        f->eax = a1;
    }
    else if(n==2){
        f->eax = a2;
    }
    else{
        int next = -1;
        for(int i=3; i<=n; i++){
            next = a1+a2;
            a0 = a1;
            a1 = a2;
            a2 = next;
        }
        f->eax = next;
    }
}


void sys_max_of_four_int(struct thread *t, struct intr_frame *f){
    int a, b, c, d;
    read_stack_int32(t->pagedir, f->esp+4, &a);
    read_stack_int32(t->pagedir, f->esp+8, &b);
    read_stack_int32(t->pagedir, f->esp+12, &c);
    read_stack_int32(t->pagedir, f->esp+16, &d);

    int temp = a;
    if (b > temp)
        temp = b;
    if (c > temp)
        temp = c;
    if (d > temp)
        temp = d;

    f->eax = temp; 
}




