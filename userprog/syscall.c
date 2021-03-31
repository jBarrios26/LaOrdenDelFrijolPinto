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
#include "threads/synch.h"
#include "threads/vaddr.h"

#include "filesys/filesys.h"
#include "filesys/file.h"

#include "devices/timer.h"

#define STDIN_FILENO 0
#define STDOUT_FILENO 1

static void syscall_handler (struct intr_frame *);
static bool verify_pointer(void *pointer); 


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
  // Revisar que el puntero sea el correcto.
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
      status = *((int*)f->esp + 1); // status is the first argument. Is stored 1 next to stack pointer (ESP). 

      // TODO: Check for valid pointers.
      // EXIT does not need to check if pointer is valid because it's argument is not a pointer. 
      exit(status);
      break;

    // *************************************************************************************************************************************************
    case SYS_EXEC:
      // Get the name of the new process, the pointer is stored in esp + 1 (first argument of SYS_EXEC),
      // ((int*)f->esp +1) returns the address of the argument, then need to dereference to then cast to char*.
      cmd_name = (char*)(*((int*)f->esp + 1)); 

      if (!verify_pointer(cmd_name)){
        exit(-1);
        return;
      }
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
      // A0 = Bytes actualy read.
      f->eax = read(fd, buffer, size);
      break;

    // *************************************************************************************************************************************************
    case SYS_REMOVE:
      file_name = (char*)(*((int*)f->esp + 1)); 

      if (!verify_pointer(file_name)){
        exit(-1);
      }
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
  //TODO: Communicate parent the status.
  //printf("\n%s sale %lu\n", cur->name, timer_ticks());
  struct thread *parent = get_thread(cur->parent);
  //printf("\n\n%d %d\n\n", cur->parent, hash_size(&parent->children));
  if (parent)
  {
    struct children_process child;
    child.pid = cur->tid;
    struct children_process *child_control = hash_entry(hash_find(&parent->children, &child.elem), struct children_process, elem);
    //printf("Se va a despertar el padre");
    if (child_control){
      child_control->finish = true; 
      child_control->status = status;
      //printf("SE va a despertar al padre");
      lock_acquire(&parent->wait_lock);
      cond_signal(&parent->wait_cond, &parent->wait_lock);
      lock_release(&parent->wait_lock);
    }
  }
  
  //TODO: Comunicate children processes that parent is exiting.
  hash_apply(&cur->children, delete_parent_from_child);
  //TODO: Free resources of the thread.

  
  thread_exit();
}

  pid_t 
exec(const char* cmd_line)
{
  tid_t child;
  struct thread *cur = thread_current();
  cur->child_load = false;
  cur->child_status = false;
  // Create the process, start to load the new process and returns the TID (PID)
  child = process_execute(cmd_line);
  if (child == -1) 
    return TID_ERROR;
  else{
    struct children_process *child_p = malloc(sizeof(struct children_process));
    //printf("\n%s crea %lu\n\n", cur->name, timer_ticks());
    child_p->pid = child;
    child_p->status = -1;
    child_p->finish = false; 
    child_p->parent_waited = false;
    hash_insert(&cur->children, &child_p->elem);
  }
  // Use a monitor that  allows the child to message his parent.
  lock_acquire(&cur->process_lock);
  // If child hasn't load then wait. To avoid race conditions.
  if(!cur->child_load)
    cond_wait(&cur->msg_parent, &cur->process_lock);

  //printf("%s %d cur  %d %d", cur->name ,cur->tid,cur->child_load, cur->child_status);
  //printf("\n\n%s, Child le aviso que fue exitoso %d", cur->name, child);
  // Check the exec_status of child.
  if (!cur->child_status)
    child = -1;

  lock_release(&cur->process_lock);

  return child;
}


bool 
create(const char* file, unsigned initial_size)
{
  bool status = false;
  lock_acquire(&file_system_lock);
  if(filesys_create(file, initial_size)) status = true;
  lock_release(&file_system_lock);
  return status;;
}

bool 
remove(const char* file)
{
  bool status = false;
  lock_acquire(&file_system_lock);
  if(filesys_remove(file)) status = true;
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

struct open_file * get_file(int fd){
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

  // Read from file
  if (fd){    
    struct open_file *opened_file = get_file(fd);
    if (opened_file != NULL){
      lock_acquire(&file_system_lock);
      struct file * temp_file = opened_file->tfiles;
      read_size = file_read(temp_file, buffer, size);
      lock_release(&file_system_lock);
    }
  }
  // Read from keyboard
  else {
    int i = 0;
    while(i < size){
      buffer[i++] = input_getc();
    }
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

  if (position < size){
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
  // Returns the next byte to be read, in bytes.
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

