#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

#include <stdio.h>
#include <syscall-nr.h>

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"


static void syscall_handler (struct intr_frame *);
static bool verify_pointer(void *pointer); 

static void halt(void);
static void exit(int status);
static pid_t exec(const char* cmd_line);
static int wait(pid_t pid);
static bool create(const char* file, unsigned initial_size);
static bool remove(const char* file);
static int open(const char* file);
static int filesize(int fd);
static int  read(int fd, void* buffer, unsigned size);
static int write (int fd, void* buffer, unsigned size);
static void seek(int fd, unsigned position);
static unsigned tell (int fd);
static void close(int fd);

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

  if (!verify_pointer(f->esp))
  {
    printf("Error en el puntero del stack");
    exit(-1);
  }
  switch (*(int*)f->esp)
  {
    case SYS_HALT:
      printf("HALT");
      break;
    case SYS_EXIT:
      status = *((int*)f->esp + 1); // status is the first argument. Is stored 1 next to stack pointer (ESP). 
      // TODO: Check for valid pointers.
      // EXIT does not need to check if pointer is valid because it's argument is not a pointer. 
      printf("EXIT STATUS %d", status);
      
      exit(status);
      break;
    case SYS_EXEC:
      printf("EXEC");
      // Get the name of the new process, the pointer is stored in esp + 1 (first argument of SYS_EXEC),
      // ((int*)f->esp +1) returns the address of the argument, then need to dereference to then cast to char*.
      cmd_name = (char*)(*((int*)f->esp + 1)); 
      if (!verify_pointer(cmd_name))
      {
        printf("puntero erroneo");
        exit(-1);
      }

      printf("%s", cmd_name);

      break;
    case SYS_WAIT:
      printf("WAIT");
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
      break;
    case SYS_WRITE:
      printf("WRITE");
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
  thread_exit ();
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

  if (is_user_vaddr(pointer) || pointer > (void*)0x08048000)
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
  process_exit();
}

static pid_t 
exec(const char* cmd_line)
{
  tid_t child;
  struct thread *cur = thread_current();
  
  // Create the process, start to load the new process and returns the TID (PID).
  child = process_execute(cmd_line);
  // Use a monitor that  allows the child to message his parent.
  lock_acquire(&cur->lock_child);
  // If child hasn't load then wait. To avoid race conditions.
  while(cur->child_load)
    cond_wait(&cur->msg_parent, &cur->lock_child);
  // Check the exec_status of child.
  if (!cur->child_status)
    child = -1;
  lock_release(&cur->lock_child);
  return child;
}

static int 
wait(pid_t pid)
{
  return -1;
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
read(int fd, void* buffer, unsigned size)
{
  return 0; 
}

static int 
write (int fd, void* buffer, unsigned size)
{
  return 0;
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