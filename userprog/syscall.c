#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

#include "lib/kernel/stdio.h"
#include "lib/string.h"
#include <stdio.h>
#include <syscall-nr.h>

#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#include "filesys/filesys.h"
#include "filesys/file.h"

#include "devices/timer.h"
#include "devices/input.h"

#define STDIN_FILENO 0
#define STDOUT_FILENO 1

static void syscall_handler (struct intr_frame *);
static bool verify_pointer(void *pointer); 
void delete_children(struct hash_elem *elem, void *aux);

void delete_parent_from_child(struct hash_elem *elem, void *aux);
void print_children(struct hash_elem *elem, void *aux);
static struct lock file_system_lock;


void 
print_children(struct hash_elem *elem, void *aux UNUSED)
{
  struct children_process *child = hash_entry(elem, struct children_process, elem);
  if (!child) printf("NULL");
  else {struct thread *child_thread = get_thread(child->pid);
  printf("%s -> ", child_thread->name);}
}



void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_system_lock);
  
}


/*
  Handles the syscalls, the input is an interrupt frame that contains the stack. 
  The intr_frame contains the SYS_CODE and up to three SYSCALL arguments.  

  f->esp = SYS_CODE;
  f->esp + 1 = arg1;
  f->esp + 2 = arg2; 
  f->esp + 3 = arg3; 

  Each argument is a pointer, so before  using its value the fuctions has to dereference the pointer. 
  
  ** SEE SYS_EXIT for an example.** 

*/
void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int status;
  char* cmd_name;
  int tid;
  char* file_name;
  
  int fd;
  unsigned position;
  unsigned size;
  void* buffer;

  if (!verify_pointer(f->esp))
  {
    exit(-1);
  }

  switch (*(int*)f->esp){
    case SYS_HALT:
      shutdown_power_off();
      break;

    // *************************************************************************************************************************************************
    case SYS_EXIT:
      if (!verify_pointer((int*)f->esp + 1))
        exit(-1);
      status = *((int*)f->esp + 1);
      
      exit(status);
      break;

    // *************************************************************************************************************************************************
    case SYS_EXEC:
      cmd_name = (char*)(*((int*)f->esp + 1)); 
      if (!verify_pointer(cmd_name))
        exit(-1);

      f->eax = exec(cmd_name);
      break;

    // *************************************************************************************************************************************************
    case SYS_WAIT:
      if (!verify_pointer((int*)f->esp + 1))
        exit(-1);
      
      tid = *((int*)f->esp + 1); 
      f->eax = process_wait(tid);
      break;

    // *************************************************************************************************************************************************
    case SYS_READ:
      if (!verify_pointer((int*)f->esp + 1))
        exit(-1);

      if (!verify_pointer((int*)f->esp + 3))
        exit(-1);

      fd = (*((int*)f->esp + 1)); 
      buffer = (char*)(*((int*)f->esp + 2));
      size = (*((int*)f->esp + 3));

      if (!verify_pointer(buffer))
        exit(-1);
      
      f->eax = read(fd, buffer, size);
      break;

    // *************************************************************************************************************************************************
    case SYS_REMOVE:
      file_name = (char*)(*((int*)f->esp + 1)); 
      if (!verify_pointer(file_name))
        exit(-1);
      
      f->eax = remove(file_name);
      break;

    // *************************************************************************************************************************************************
    case SYS_OPEN:
      file_name = (void*)(*((int*)f->esp + 1)); 
      if (!verify_pointer(file_name))
        exit(-1);
      
      f->eax = open(file_name);
      break;

    // *************************************************************************************************************************************************
    case SYS_FILESIZE:
      if (!verify_pointer((int*)f->esp + 1))
        exit(-1);
      
      fd = (*((int*)f->esp + 1)); 
      f->eax = filesize(fd);
      break;

    // *************************************************************************************************************************************************
    case SYS_CREATE: 
      if (!verify_pointer((int*)f->esp + 1))
        exit(-1);
      
      if (!verify_pointer((int*)f->esp + 2))
        exit(-1);
      
      file_name = (char*)(*((int*)f->esp + 1)); 
      if (!verify_pointer(file_name))
        exit(-1);

      size = (*((int*)f->esp + 2));
      f->eax = create(file_name, size);
      break;

    // *************************************************************************************************************************************************
    case SYS_WRITE:
      if (!verify_pointer((int*)f->esp + 1))
        exit(-1);

      if (!verify_pointer((int*)f->esp + 3))
        exit(-1);

      fd = *((int*)f->esp + 1);
      buffer = (void*)(*((int*)f->esp + 2));
      size = *((int*)f->esp + 3);

      if (!verify_pointer(buffer))
        exit(-1);
      f->eax = write(fd, buffer, size);
      break;

    // *************************************************************************************************************************************************
    case SYS_SEEK:
      if (!verify_pointer((int*)f->esp + 1))
        exit(-1);
      
      if (!verify_pointer((int*)f->esp + 2))
        exit(-1);
          
      fd = *((int*)f->esp + 1);
      position = *((int*)f->esp + 2);

      seek(fd, position);
      break;

    // *************************************************************************************************************************************************
    case SYS_TELL:
      if (!verify_pointer((int*)f->esp + 1))
        exit(-1);

      fd = *((int*)f->esp + 1);

      f->eax = tell(fd);
      break;

    // *************************************************************************************************************************************************
    case SYS_CLOSE:
      if (!verify_pointer((int*)f->esp + 1))
        exit(-1);
      
      fd = *((int*)f->esp + 1); 
      close(fd);
      break;
  }
}


/*
  Checks if pointer meets this two conditions: 
    1. Pointer is between PHYS_BASE and 0x08048000
    2. The pointer is allocated in thread page.

*/
bool 
verify_pointer(void *pointer)
{
  struct thread *cur = thread_current(); 
  bool valid = true;

  if (!is_user_vaddr(pointer) || pointer == NULL)
      return false;
  
  if (pagedir_get_page(cur->pagedir, pointer) == NULL)
      return false;
  
  return valid; 
}


void 
exit(int status)
{
  struct thread *cur = thread_current();
  // Process termination message. 
  printf("%s: exit(%d)\n", cur->name, status);
  
  struct thread *parent = get_thread(cur->parent);
  if (parent)
  {
    struct children_process child;
    child.pid = cur->tid;
    struct children_process *child_control = hash_entry(hash_find(&parent->children, &child.elem), struct children_process, elem);
    if (child_control){
      lock_acquire(&parent->wait_lock);
      child_control->finish = true; 
      child_control->status = status;
      cond_signal(&parent->wait_cond, &parent->wait_lock);
      lock_release(&parent->wait_lock);
    }
  }
  
  hash_apply(&cur->children, delete_parent_from_child);
  hash_destroy(&cur->children, delete_children);

  struct lock *lock;
  while (!list_empty(&cur->locks))
  {
    lock = list_entry(list_pop_back(&cur->locks), struct lock, elem);
    lock_release(lock);
  }

  if (cur->fd_exec != -1)
    close(cur->fd_exec);

  struct list_elem *iter = list_begin(&cur->files); 
  while (!list_empty(&cur->files))
  {   
    struct open_file *op_file = list_entry(iter, struct open_file, at);
    iter = list_next(iter);
    close(op_file->fd);
  }
  
  thread_exit();
}

pid_t 
exec(const char* cmd_line)
{
  tid_t child;
  struct thread *cur = thread_current();
  struct children_process *child_p = NULL;
  cur->child_load = false;
  cur->child_status = false;
  
  child = process_execute(cmd_line);
  
  if (child == -1) 
    return TID_ERROR;
  else{
    child_p = malloc(sizeof(struct children_process));
    child_p->pid = child;
    child_p->status = -1;
    child_p->finish = false; 
    child_p->parent_waited = false;
    hash_insert(&cur->children, &child_p->elem);
  }
  
  if(!cur->child_load)
    sema_down(&cur->exec_sema);;

  if (!cur->child_status){
    hash_delete(&cur->children,&child_p->elem);
    free(child_p);
    child = -1;
  }

  return child;
}


bool 
create(const char* file, unsigned initial_size)
{
  bool status = false;
  
  lock_acquire(&file_system_lock);
  status = filesys_create(file, initial_size);
  lock_release(&file_system_lock);
  
  return status;;
}

bool 
remove(const char* file)
{
  bool status = false;
  
  lock_acquire(&file_system_lock);
  status = filesys_remove(file);
  lock_release(&file_system_lock);
  
  return status;
}

int 
open(const char* file)
{
  struct thread *cur = thread_current();
  struct file *last_file = NULL;
  struct file *file_op = NULL;

  struct list_elem *iter = list_begin(&all_files);
  while (iter != list_end(&all_files))
  {
    struct open_file *op_file = list_entry(iter, struct open_file, af);
    if (strcmp(file, op_file->name) == 0){
      last_file = op_file->tfiles;
      break;
    }
    iter = list_next(iter);
  }
  
  lock_acquire(&file_system_lock);
  if (last_file != NULL)
    file_op = file_reopen(last_file);
  else 
    file_op = filesys_open(file);
  lock_release(&file_system_lock);
  
  if(file_op != NULL){
    struct open_file *op_file = malloc(sizeof(struct open_file));
    op_file->fd = cur->fd_next++;
    op_file->tfiles = file_op;
    op_file->name = (char*)file;
    list_push_back(&cur->files, &op_file->at);
    list_push_back(&all_files, &op_file->af);
    return op_file->fd;
  }
  return -1;
}

int 
filesize(int fd)
{
  int size = 0;

  struct open_file *opened_file = get_file(fd);
  struct file *temp_file = opened_file->tfiles;
  
  lock_acquire(&file_system_lock);
  size = file_length(temp_file);
  lock_release(&file_system_lock);

  return size;
}

struct open_file 
*get_file(int fd){
    struct list * opfiles = &thread_current()->files;
    struct list_elem *files= list_begin(opfiles);    
    while (files != list_end(opfiles))
    {   
      struct open_file *opfile  = list_entry(files, struct open_file, at);
      if (opfile->fd == fd)
        return opfile;
      files = list_next(files);
    }
    return NULL;
}


int 
read(int fd, char* buffer, unsigned size)
{
  int read_size = -1;

  if (fd){    
    struct open_file *opened_file = get_file(fd);
    if (opened_file != NULL){
      lock_acquire(&file_system_lock);
      struct file * temp_file = opened_file->tfiles;
      read_size = file_read(temp_file, buffer, size);
      lock_release(&file_system_lock);
    }
  }
  else {
    int i = 0;
    while((unsigned int)i < size)
      buffer[i++] = input_getc();
    read_size = size;
  }
  return read_size;
}

int 
write (int fd, void* buffer, unsigned size)
{
  if (fd == STDIN_FILENO){
    return 0;
  }
  else if (fd == STDOUT_FILENO){
    putbuf((char*) buffer, size);
    return size;
  }else{
    struct open_file *opfile = get_file(fd);
    int written_bytes = 0;
    if (opfile == NULL)
      return 0;
    lock_acquire(&file_system_lock);
    written_bytes = file_write(opfile->tfiles, buffer, size);
    lock_release(&file_system_lock);
    return written_bytes;
  }
}

void 
seek(int fd, unsigned position)
{
  int size = filesize(fd);

  struct open_file *opened_file = get_file(fd);
  struct file *temp_file = opened_file->tfiles;

  if (position < (unsigned int)size){
    lock_acquire(&file_system_lock);
    file_seek(temp_file, position);
    lock_release(&file_system_lock);
  }
  else {
    exit(-1);
  }
}

  unsigned 
tell(int fd)
{
  struct open_file *opened_file = get_file(fd);
  struct file *temp_file = opened_file->tfiles;
  
  lock_acquire(&file_system_lock);
  unsigned ret = file_tell(temp_file);
  lock_release(&file_system_lock);
  
  return ret;
}

  void 
close(int fd)
{
  struct open_file *openfile = get_file(fd);
  if(openfile != NULL){
    lock_acquire(&file_system_lock);
    file_close(openfile->tfiles);
    lock_release(&file_system_lock);
    list_remove(&openfile->at);
    list_remove(&openfile->af);
    free(openfile);
  }
}

void 
delete_parent_from_child(struct hash_elem *elem, void *aux UNUSED)
{
  struct children_process *child = hash_entry(elem, struct children_process, elem);
  struct thread *child_thread = get_thread(child->pid);

  if (child_thread)
  {
    child_thread->parent = 0;
  }

}

void 
delete_children(struct hash_elem *elem, void *aux UNUSED)
{
  struct children_process *child = hash_entry(elem, struct children_process, elem);
  free(child);
}
