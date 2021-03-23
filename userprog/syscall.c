#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

#include "lib/kernel/stdio.h"
#include <stdio.h>
#include <syscall-nr.h>

#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#include "filesys/filesys.h"
#include "filesys/file.h"

#define STDIN_FILENO 0
#define STDOUT_FILENO 1

static void syscall_handler (struct intr_frame *);
static bool verify_pointer(void *pointer); 
int write_to_console(void* buffer, unsigned size);

static void halt(void);
static void exit(int status);
static pid_t exec(const char* cmd_line);
static int wait(pid_t pid);
static bool create(const char* file, unsigned initial_size);
static bool remove(const char* file);
static int open(const char* file);
static int filesize(int fd);
static int read(int fd, char* buffer, unsigned size);
static int write (int fd, void* buffer, unsigned size);
static void seek(int fd, unsigned position);
static unsigned tell (int fd);
static void close(int fd);

void delete_parent_from_child(struct hash_elem *elem, void *aux);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // Revisar que el puntero sea el correcto.
  int status;
  char* cmd_name;
  int tid;
  
  int fp;
  char* buffer;
  unsigned size;

  if (!verify_pointer(f->esp))
  {
    exit(-1);
  }
  switch (*(int*)f->esp)
  {
    case SYS_HALT:
      shutdown_power_off();
      break;
      
    case SYS_EXIT:
      status = *((int*)f->esp + 1); // status is the first argument. Is stored 1 next to stack pointer (ESP). 
      // TODO: Check for valid pointers.
      // EXIT does not need to check if pointer is valid because it's argument is not a pointer. 
      exit(status);
      break;
    case SYS_EXEC:
      // Get the name of the new process, the pointer is stored in esp + 1 (first argument of SYS_EXEC),
      // ((int*)f->esp +1) returns the address of the argument, then need to dereference to then cast to char*.
      cmd_name = (char*)(*((int*)f->esp + 1)); 
      if (!verify_pointer(cmd_name))
      {
        printf("puntero erroneo");
        exit(-1);
        return;
      }

      printf("%s", cmd_name);
      f->eax = exec(cmd_name);
      break;
    case SYS_WAIT:
      tid = *((int*)f->esp + 1); 
      process_wait(tid);
      break;
    case SYS_CREATE:
      printf("CREATE");
      break;
    case SYS_REMOVE:
      printf("REMOVE");
      break;
    case SYS_OPEN:
      printf("OPEN");
      break;
    case SYS_FILESIZE:
      printf("FILESIZE");
      break;
    case SYS_READ: 
      printf("READ");

      fp = (*((int*)f->esp + 1)); 
      buffer = (char*)(*((int*)f->esp + 2));
      size = (*((int*)f->esp + 3));

      if (!verify_pointer(buffer))
      {
        printf("puntero erroneo");
        exit(-1);
      }

      // AO = Bytes actualy read.
      f->eax = read(fp, buffer, size);

      printf("bytes: %d", f->eax);

      break;
    case SYS_WRITE:
      fp = *((int*)f->esp + 1);
      buffer = (void*)(*((int*)f->esp + 2));
      size = *((int*)f->esp + 3);

      if (!verify_pointer(buffer))
      {
        printf("Puntero Erroneo");
        exit(-1);
      }

      f->eax = write(fp, buffer, size);
      break;
    case SYS_SEEK:
      printf("SEEK");
      break;
    case SYS_TELL:
      printf("TELL");
      break;
    case SYS_CLOSE:
      printf("CLOSE");
      break;
  }
}


/*
  Checks if pointer meets this two conditions: 
    1. Pointer is between PHYS_BASE and 0x08048000
    2. The pointer is allocated in thread page.

*/
static bool 
verify_pointer(void *pointer)
{
  struct thread *cur = thread_current(); 
  bool valid = true;

  if (!is_user_vaddr(pointer) || pointer == NULL)
    valid = false;
  
  if (pagedir_get_page(cur->pagedir, pointer) == NULL)
    valid = false;
  
  return valid; 
}

static void 
halt(void)
{
  return;
}

static void 
exit(int status)
{
  struct thread *cur = thread_current();
  // Process termination message. 
  printf("%s: exit(%d)\n", cur->name, status);

  //TODO: Communicate parent the status.
  struct thread *parent = get_thread(cur->parent);
  if (parent)
  {
    struct children_process child;
    child.pid = cur->tid;
    struct children_process *child_control = hash_entry(hash_find(&parent->children, &child.elem), struct children_process, elem);
    if (child_control){
      child_control->finish = true; 
      child_control->status = status;
      lock_acquire(&parent->process_lock);
      cond_signal(&parent->msg_parent, &parent->process_lock);
      lock_release(&parent->process_lock);
    }
  }
  
  //TODO: Comunicate children processes that parent is exiting.
  hash_apply(&cur->children, delete_parent_from_child);
  //TODO: Free resources of the thread.

  
  thread_exit();
}

static pid_t 
exec(const char* cmd_line)
{
  tid_t child;
  struct thread *cur = thread_current();
  // Create the process, start to load the new process and returns the TID (PID).
  child = process_execute(cmd_line);
  // Use a monitor that  allows the child to message his parent.
  lock_acquire(&cur->process_lock);
  // If child hasn't load then wait. To avoid race conditions.
  while(!cur->child_load)
    cond_wait(&cur->msg_parent, &cur->process_lock);
  // Check the exec_status of child.
  if (!cur->child_status)
    child = -1;
  else
  {
    struct children_process *child_p = malloc(sizeof(struct children_process));
    child_p->pid = child;
    child_p->status = -1;
    child_p->finish = false; 
    hash_insert(&cur->children, &child_p->elem);
  }
  
  cur->child_load = false;
  cur->child_status = false;
  lock_release(&cur->process_lock);
  return child;
}

static int 
wait(pid_t pid)
{
  return process_wait(pid);
}

static bool 
create(const char* file, unsigned initial_size)
{
  return false;
}

static bool 
remove(const char* file)
{
  return false;
}

static int 
open(const char* file)
{
  return 0;
}

static int 
filesize(int fd)
{
  return 0;
}

static int 
read(int fd, char* buffer, unsigned size)
{
  int read_size = -1;

  // Read from keyboard
  if (fd){
    // TODO Agree with Chato about the file_reading stuff
    // if (readOK){
    //   read_size = file_read(file_name, buffer, size);
    // }
  }
  // Read from file
  else {
    int i = 0;
    while(i < size){
      buffer[i++] = input_getc();
    }
    read_size = size;
  }

  return read_size;
}

static int 
write (int fd, void* buffer, unsigned size)
{
  switch (fd)
  {
    case STDIN_FILENO:
      return 0;
    case STDOUT_FILENO:
      putbuf((char*) buffer, size);
      break;
    default:
      break;
  }
}

int 
write_to_console(void *buffer, unsigned size)
{
  
}

static void 
seek(int fd, unsigned position)
{
  return; 
}

static 
unsigned tell (int fd)
{
  return 0;
}

static void 
close(int fd)
{
  return;
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

