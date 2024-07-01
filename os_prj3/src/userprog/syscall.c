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
#include "filesys/file.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);



void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_lock);
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
    /* Additional */
    case SYS_FIBONACCI:
      sys_fibonacci(t, f);
      break;
    case SYS_MAX_OF_FOUR_INT:
      sys_max_of_four_int(t, f);
      break;
    /* File Related */
    case SYS_CREATE:
      sys_create(t, f);
      break;
    case SYS_REMOVE:
      sys_remove(t, f);
      break;
    case SYS_OPEN:
      sys_open(t, f);
      break;
    case SYS_CLOSE:
      sys_close(t, f);
      break;
    case SYS_FILESIZE:
      sys_filesize(t, f);
      break;
    case SYS_WRITE:
      sys_write(t, f);
      break;
    case SYS_READ:
      sys_read(t, f);
      break;
    case SYS_SEEK:
      sys_seek(t, f);
      break;
    case SYS_TELL:
      sys_tell(t, f);
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
        list_remove(&child_t->thread_item.elem);
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
        return;
    }
    else if(fd == 0 || fd > 128 || fd < 0){
        f->eax = -1;
        return;
    }

    // fd not 0, 1
    struct file *cur_file = t->fd_table[fd];
    if(cur_file == NULL){
        f->eax = -1;
        return;
    }

    lock_acquire(&file_lock);
    f->eax = file_write(cur_file, buffer, size);
    lock_release(&file_lock);
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
        return;
    }
    else if (fd == 1 || fd > 128 || fd < 0){
        f->eax = -1;
        return;
    }

    // fd not 0, 1
    struct file *cur_file = t->fd_table[fd];
    if(cur_file == NULL){
        f->eax = -1;
        return;
    }

    lock_acquire(&file_lock);
    f->eax = file_read(cur_file, buffer, size);
    lock_release(&file_lock);
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

void sys_create(struct thread *t, struct intr_frame *f){
    char *file;
    unsigned initial_size;
    read_stack_pointer(t->pagedir, f->esp+4, (void**)&file);
    read_stack_uint32(t->pagedir, f->esp+8, &initial_size);

    lock_acquire(&file_lock);
    f->eax = filesys_create(file, initial_size);
    lock_release(&file_lock);
}

void sys_remove(struct thread *t, struct intr_frame *f){
    char *file;
    read_stack_pointer(t->pagedir, f->esp+4, (void**)&file);
    
    lock_acquire(&file_lock);
    f->eax = filesys_remove(file);
    lock_release(&file_lock);
}

void sys_open(struct thread *t, struct intr_frame *f){
    char *file;
    read_stack_pointer(t->pagedir, f->esp+4, (void**)&file);

    lock_acquire(&file_lock);
    struct file *opened_file = filesys_open(file);
    lock_release(&file_lock);

    if(opened_file == NULL){
        f->eax = -1;
    }
    else{
        int i;
        // checks for the lowest file descriptor unused
        for(i=2; i<128; i++){
            if(t->fd_table[i] == NULL){
                break;
            }
        }
        // don't have more space in fd table
        if(i==128){
            f->eax = -1;
            return;
        }
        // i is the lowest fd unused
        t->fd_table[i] = opened_file;
        f->eax = i;
    }
    return;
}

void sys_close(struct thread *t, struct intr_frame *f){
    int fd;
    read_stack_int32(t->pagedir, f->esp+4, &fd);

    // error in fd
    if(fd > 128 || fd < 0){
        f->eax = -1;
        return;
    }

    struct file *cur_file = t->fd_table[fd];
    if(cur_file == NULL){
        return;
    }

    lock_acquire(&file_lock);
    file_close(cur_file);
    lock_release(&file_lock);
    t->fd_table[fd] = NULL;
}

void sys_filesize(struct thread *t, struct intr_frame *f){
    int fd;
    read_stack_int32(t->pagedir, f->esp+4, &fd);

    // error in fd
    if(fd > 128 || fd < 0){
        f->eax = -1;
        return;
    }

    struct file *cur_file = t->fd_table[fd];
    if(cur_file == NULL){
        f->eax = -1;
        return;
    }

    lock_acquire(&file_lock);
    f->eax = file_length(cur_file);
    lock_release(&file_lock);
}

void sys_seek(struct thread *t, struct intr_frame *f){
    int fd;
    unsigned position;
    read_stack_int32(t->pagedir, f->esp+4, &fd);
    read_stack_uint32(t->pagedir, f->esp+8, &position);

    // error in fd
    if(fd > 128 || fd < 0){
        return;
    }

    struct file *cur_file = t->fd_table[fd];
    if(cur_file == NULL){
        return;
    }

    lock_acquire(&file_lock);
    file_seek(cur_file, position);
    lock_release(&file_lock);
}

void sys_tell(struct thread *t, struct intr_frame *f){
    int fd;
    read_stack_int32(t->pagedir, f->esp+4, &fd);

    // error in fd
    if(fd > 128 || fd < 0){
        f->eax = -1;
        return;
    }

    struct file *cur_file = t->fd_table[fd];
    if(cur_file == NULL){
        f->eax = -1;
        return;
    }

    lock_acquire(&file_lock);
    f->eax = file_tell(cur_file);
    lock_release(&file_lock);
}
