#include "userprog/process.h"
#include "syscall.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"

#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static int reverse_args(char *args, char *reversed, char *aux);
static void push_args(char *args, void **esp, void *address[]);
static int push_name(char *name, void **esp, void *address[], int argc);
static void push_addresses(void **esp, void *adress[], int argc); 
static void align_mem(void **esp);

static unsigned
children_hash (const  struct hash_elem *elem, void *aux UNUSED)
{
  const struct children_process *child  = hash_entry(elem, struct children_process, elem);
  return hash_int(child->pid);
}

static bool childres_hash_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
  const struct children_process *a_ = hash_entry (a, struct children_process, elem);
  const struct children_process *b_ = hash_entry (b, struct children_process, elem);
  return a_->pid < b_->pid;
}


/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
{
  char *fn_copy, *name, *save_ptr, *args;
  tid_t tid;
  struct thread *cur = thread_current();

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  args = palloc_get_page (PAL_USER | PAL_ZERO);
  if (fn_copy == NULL || args == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);
  /* Get filename */
  name = strtok_r (fn_copy, " ", &save_ptr);
  strlcpy(args, save_ptr, strlen(file_name) - strlen(name));
  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (name, PRI_DEFAULT, start_process,args);

  if (tid == -1){
    palloc_free_page(fn_copy);
    palloc_free_page (args);
    return TID_ERROR;
  }

  /* If children hash map hasn't been initialized, then the parent process is really a kernel thread that
  is executing a new process and his children hash map has not been initialized. */
  if (!thread_current()->children_init)
  {
    hash_init(&thread_current()->children, children_hash, childres_hash_less, NULL);
    /* Create a new children_process control struct to keep control of children process. */
    struct children_process *child_p = malloc(sizeof(struct children_process));
    child_p->pid = tid;
    child_p->status = -1;
    child_p->finish = false;
    child_p->parent_waited = false;
    hash_insert(&cur->children, &child_p->elem);
    thread_current()->children_init = true;
  }
  palloc_free_page (fn_copy);
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  struct thread* cur = thread_current();
  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page(file_name_);

  /* Need to check if load was successful and signal the parent thread.
     Need to use the synchronization variables defined on the parent thread.
  */
  hash_init(&cur->children, children_hash, childres_hash_less, NULL);
  cur->children_init = true;
  struct thread* parent = get_thread(cur->parent);
  // struct children_process child;
  // child.pid = cur->tid;
  // struct hash_elem *child_elem = hash_find(&cur->children, &child.elem);

  if (parent != NULL  ){
  /* Acquire lock from parent to modify state variables and signal him.*/
    if (!success)
    {
      parent->child_load = true;
      sema_up(&parent->exec_sema);
      thread_exit();
    }else
    {
      parent->child_status = true;
      parent->child_load = true;
      sema_up(&parent->exec_sema);
    }
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid)
{
  struct thread *cur;
  cur = thread_current();

  /*  get the child process from parent children hash table. */
  struct children_process child;
  child.pid = child_tid;

 struct hash_elem *child_elem = hash_find(&cur->children, &child.elem);
  /*
    If child_elem is NULL then this child_tid doesn't map to a child process of current thread, return -1.
    If child_elem is not NULL then check if child has already finish, return child process exit status.
  */
  if (child_elem == NULL)
  {
    return -1;
  }
  /*
    If finish is true then child process maybe have exited on their own or current may have already waited for him once.
  */
  struct children_process *child_control;
  child_control = hash_entry(child_elem, struct children_process, elem);

  /* Check if the current has already waited for this child once. */
  if (child_control->parent_waited)
  {
    return -1;
  }

  /* Check if child has already finish, then return its status. */
  if (child_control->finish){
    child_control->parent_waited = true;
    return child_control->status;
  }

  /* Wait for child to finish executing. Parent process waits here and child process signals from exit SYSCALL. */
  lock_acquire(&cur->wait_lock);
    while (!child_control->finish)
    {
      cond_wait(&cur->wait_cond, &cur->wait_lock);
    }
  lock_release(&cur->wait_lock);
  return child_control->status;
}

/* Free the current process's resources. */
void
process_exit (void)
{

  struct thread *cur = thread_current ();
  uint32_t *pd;
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */

  pd = cur->pagedir;
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }


}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, char *args,char *name);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  char *file_name_ = (char*) file_name;
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (t->name);

  if (file == NULL)
    {
      printf ("load: %s: open failed\n", t->name);
      goto done;
    }
  /* create open file struct and push it to thread files and all files list */
  struct open_file *op_file = malloc(sizeof(struct open_file));
  file_deny_write(file);
  op_file->fd = t->fd_next++;
  op_file->tfiles = file;
  op_file->name = t->name;
  list_push_back(&t->files, &op_file->at);
  list_push_back(&all_files, &op_file->af);
  t->fd_exec = op_file->fd;
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      printf ("load: %s: error loading executable\n", t->name);
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, file_name_, t->name))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */

  /*
    if load was not sucessful then close the file.
    else it stays open until process exits.
  */
  if (!success && file != NULL){
    close(op_file->fd);
  }

  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;


      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false;
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable))
        {
          palloc_free_page (kpage);
          return false;
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, char *args, char *name)
{
  uint8_t *kpage;
  // char *token;
  // char *save_ptr;
  // int start_stack = 0;
  bool success = false;
  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
  {
    success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
    if (success){

      *esp = PHYS_BASE;
      /* SETUP args in stack*/
      char *reverse, *aux = "";
      reverse = palloc_get_page(PAL_USER | PAL_ZERO);
      aux = palloc_get_page(PAL_USER |PAL_ZERO);

      if (reverse == NULL || aux == NULL){
        success = false;
        palloc_free_page(reverse);
        palloc_free_page(aux);
        return success;
      }

      int argc = reverse_args(args, reverse, aux); 

      void *addresses[argc + 1];
      
      push_args(reverse, esp, addresses); 
      
      argc = push_name(name, esp, addresses, argc); 

      // Align memory.
      align_mem(esp); 

      // Write argv addresses.
      *esp -= 4;
      memset (*esp, 0, 4);

      push_addresses(esp, addresses, argc); 

      // push argv[0] address
      void *argv0 = *esp;
      *esp -= sizeof(char**);
      memcpy(*esp, &argv0, sizeof(char**));

      *esp-=sizeof(argc);
      memcpy(*esp, &argc, 4);

      *esp -= sizeof(void*);
      memset(*esp, 0, sizeof(void*));

      palloc_free_page(aux);
      palloc_free_page(reverse);
    }
    else
      palloc_free_page (kpage);
  }

  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

/* Take the args string and reverse de agrs order. 
   Input: arg0 arg1 arg2 
   Output: arg2 arg1 arg0   
*/
static int reverse_args(char *args, char *reverse, char *aux)
{
  char *token, *save_ptr; 
  int argc = 0; 
  for (token = strtok_r (args, " ", &save_ptr); token != NULL;
        token = strtok_r (NULL, " ", &save_ptr))
        {
          strlcpy(aux, token, PGSIZE);
          strlcat(aux, " ", PGSIZE);
          strlcat(aux, reverse, PGSIZE);
          strlcpy(reverse, aux, PGSIZE);
          ++argc; 
        }
  return argc;
}

/* Push  the args strings to the stack. Expects that the args list has already been reversed.*/
static void push_args(char *args, void **esp, void *address[])
{
  char *token, *save_ptr;
  int argc = 0;
  if (strlen(args) > 0){
    for (token = strtok_r (args, " ", &save_ptr); token != NULL;
          token = strtok_r (NULL, " ", &save_ptr))
    {
      *esp-= strlen(token)+1;
      memcpy(*esp, token, strlen(token)+1);
      address[argc++] = *esp;
    }
  }
}

/* Push the executable name to the stack. */
static int push_name(char *name, void **esp, void *address[], int argc)
{
  *esp-= strlen(name)+1;
  memcpy(*esp, name, strlen(name)+1);
  address[argc++] = *esp;
  return argc;
}

/* Push the addresses of the arguments to the stack. This simulates the array argv*/
static void push_addresses(void **esp, void *address[], int argc)
{
  int i = 0;
  while (i < argc)
  {
    *esp -= sizeof(char *);
    memcpy(*esp,  &address[i++], sizeof(char*));
  }
}

/* Aligns the memory because pushing the strings to the stack could leave the stack pointer unaligned. */
static void align_mem(void **esp)
{
  int mem_align =  (-1 * (int)*esp) % 4;
  *esp -= mem_align;
  memset(*esp, 0, mem_align);
}