#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

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
  Se encarga de manejar las llamadas a los syscalls: 
  El intr_frame tiene los argumentos y el codigo del syscall. 

  f->esp = SYS_CODE;
  f->esp + 1 = arg1;
  f->esp + 2 = arg2; 
  f->esp + 3 = arg3; 

  El mayor número de argumentos que tiene un SYSCALL es de 3.

*/
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // Revisar que el puntero sea el correcto.
  switch (*(int*)f->esp)
  {
    case SYS_HALT:
      printf("HALT");
      break;
    case SYS_EXIT:
      printf("EXIT");
      break;
    case SYS_EXEC:
      printf("EXEC");
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
