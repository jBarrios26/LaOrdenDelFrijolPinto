#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "userprog/exception.h"

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

// #ifdef VM
#include "vm/spage.h"
#include "vm/frame.h"
// #endif

#define STDIN_FILENO 0
#define STDOUT_FILENO 1

static void syscall_handler (struct intr_frame *);
static bool verify_pointer(void *pointer); 
void delete_children(struct hash_elem *elem, void *aux);

void delete_parent_from_child(struct hash_elem *elem, void *aux);
void print_children(struct hash_elem *elem, void *aux);

static void  preload(void *buffer, size_t size); 
int calc_pages(void *upage, size_t size);



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
  struct thread *cur = thread_current(); 
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

  cur->esp = f->esp;
  cur->on_syscall = true;

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
      if (!verify_pointer((int*)f->esp + 1)){
        exit(-1);
      }

      if (!verify_pointer((int*)f->esp + 3)){
        exit(-1);
      }

      

      fd = (*((int*)f->esp + 1)); 
      buffer = (char*)(*((int*)f->esp + 2));
      size = (*((int*)f->esp + 3));

      if (!is_user_vaddr(buffer) || buffer == NULL){
        exit(-1);
      }
      
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
      if (!verify_pointer(file_name)){
        exit(-1);
      }
      
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
    case SYS_MMAP: 
      if (!verify_pointer((int*)f->esp + 1))
        exit(-1);

      buffer = (void*)(*((int*)f->esp + 2));

      fd = *((int*)f->esp + 1);
      buffer = (void*)(*((int*)f->esp + 2));

      f->eax = mmap(fd, buffer);
      break;
    case SYS_MUNMAP:
      if (!verify_pointer((int*)f->esp + 1))
          exit(-1);  

      fd = *((int*)f->esp + 1);
      unmap((mapid_t)fd);
      break;
  }

  cur->on_syscall = false; 
  cur->esp = NULL; 
  cur->fault_addr = NULL;
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

  #ifdef VM  
  struct hash_iterator e; 
  hash_first(&e, &cur->mm_table); 
  struct hash_elem *mmiter = hash_next(&e);
  while (mmiter)
  {
    struct mmap_file *mmfile = hash_entry(hash_cur(&e), struct  mmap_file, elem ); 
    mmiter = hash_next(&e); 
    unmap(mmfile->mapping); 
  }
  #endif

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

/* 
  Searches fd file in current thread file list and returns open_file pointer if fd is on the list. 
  If fd file is not on the list, then return NULL pointer.
*/
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

  struct thread *cur = thread_current(); 
  cur->fault_addr = buffer;

  preload(buffer, size);
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
  #ifdef VM
  unpin_frames(cur);
  #endif
  return read_size;
}

int 
write (int fd, void* buffer, unsigned size)
{

  struct thread *cur = thread_current(); 
  cur->fault_addr = buffer;
  preload(buffer, size);
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


bool check_overlap(struct hash *mmtable, void *base, int length);
bool check_overlap_existing(void *base, int length); 

/* Maps a fd file to memory. */
mapid_t 
mmap(int fd, void  *addr)
{
  struct thread *cur = thread_current();
  
  if (!cur)
    return -1;
  /* Check if fd is STDIN OR STDOUT.*/
  if (fd == 0 || fd == 1)
    return -1;
  
  /* Check if fd file is in thread file list */
  struct open_file *mfile = get_file(fd);
  if (mfile == NULL)
    return -1;
  

  /* Check if addr is valid. This means if addr is not NULL, is not zero and it is aligned with page limits. */
  if (addr == NULL)
    return -1; 
  
  if (addr == 0x0)
    return -1;
  
  if (pg_ofs(addr) != 0)
    return -1;
  
  off_t length = file_length(mfile->tfiles); 
  /* Check if is a zero byte file (length is zero.) */
  if (length == 0)
    return -1; 

  /* Check if this file overlaps with already memory mapped files. */
  if (check_overlap(&cur->mm_table, addr,length))
    return -1; 

  /* Checks if this file overlaps with stack or executable pages.*/
  if (check_overlap_existing(addr, length))
    return -1; 
  
  /*At this point file is ready and safe to map*/
  struct file *mem_file = file_reopen(mfile->tfiles); 

  /* First ask for pages in the supplementary page table, we don't ask for frames because memory mapped files have to be lazy loaded.*/
  int read_bytes = length; 
  off_t ofs = 0; 
  void *base = addr; 
  int pages = 0;
  while (read_bytes > 0)
  {
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    if (!get_file_page(mem_file, ofs, page_read_bytes, page_zero_bytes, true, MMFILE,  base))
          PANIC("No se logro insertar a la SPT"); 
    

    ofs += page_read_bytes;
    base += PGSIZE; 
    read_bytes -= page_read_bytes;
    pages++; 
  }
  /* At this point the whole file is already mapped in the supplementary page table and will be loaded when userprog triggers a page fault.  */


  cur->mapid = ++cur->mapid;
  struct mmap_file *mapping = malloc(sizeof(struct mmap_file));
  if (!mapping)
    return -1; 
  
  mapping->base = addr;
  mapping->mapping = cur->mapid; 
  mapping->file = mem_file;
  mapping->length = length; 
  mapping->page_span = pages;
  hash_insert(&cur->mm_table, &mapping->elem); 

  return cur->mapid; 
}

/*
  Checks if the new file map overlaps with existing mappings. For overlap to exists one of the following conditions needs to be true:

    1. e.base <= new.base + new.length <= e.base + e.length
    2. e.base <= new.base <= e.base + e.lengtg

  If overlap is found returns true.   

*/
bool 
check_overlap(struct hash *mmtable, void *base, int length){
  struct hash_iterator e; 
  hash_first(&e, mmtable); 
  while (hash_next(&e))
  {
    struct mmap_file *mmfile = hash_entry(hash_cur(&e), struct  mmap_file, elem ); 
    // printf("Procesando %d: (%p %p) , (%x %x )\n", mmfile->mapping, mmfile->base, base, mmfile->length, length);
    if (mmfile->base <= base + length && 
          base + length <= mmfile->base + mmfile->length)
      return true;

    if (mmfile->base <= base &&
          base <= mmfile->base + mmfile->length)
      return true;
  }
  return false; 
}

/*
  Checks if the mapped file doesn't overlap with already virtual pages of the stack or executable file. 

  Returns true is overlap exists. 
*/
bool 
check_overlap_existing(void *base, int length){
  struct thread *cur = thread_current();
  void *addr = base;
  while (addr <= base + length)
  {
    if (lookup_page(cur, addr))
      return true;
    
    addr += PGSIZE;
  }
  return false; 
}



void 
unmap(mapid_t mapping)
{

  struct thread *cur = thread_current();
  struct mmap_file e;
  e.mapping = mapping;

  struct hash_elem *map_elem = hash_delete(&cur->mm_table, &e.elem);
  if (!map_elem)
    return;

  struct mmap_file *mmfile = hash_entry(map_elem, struct mmap_file, elem);

  void *addr = mmfile->base; 
  size_t read_bytes = mmfile->length; 
  // preload(addr, read_bytes); 
  while(read_bytes > 0)
  {
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;

    struct spage_entry *page = lookup_page(cur, addr);
    if (!page)
      return; 
    
    if (!page->loaded){
      remove_SPentry(&cur->sup_table, addr);
      addr += PGSIZE; 
      continue;
    }
    /* Check if page was modified. */

    if (page->loaded && pagedir_is_dirty(cur->pagedir, page->upage)){
      lock_acquire(&file_system_lock);
        file_seek(page->file->file, page->file->ofs); 
        file_write(page->file->file, addr + page->file->ofs , page->file->read_bytes);
      lock_release(&file_system_lock);
    }
    
    pagedir_clear_page(cur->pagedir, page->upage);
    struct frame_entry *fte = lookup_uframe(cur, page->upage);
    destroy_frame(fte->frame);
    remove_SPentry(&cur->sup_table, addr);
    read_bytes -= page_read_bytes;
    addr += PGSIZE;
  } 
  lock_acquire(&file_system_lock);
  file_close(mmfile->file);
  lock_release(&file_system_lock);
  free(mmfile);


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


int calc_pages(void *upage, size_t size)
{
  size_t allocated = 0;
  size_t last_increment = 0;
  int pages = 0;
  while (allocated < size )
  {
    if (allocated + PGSIZE > size){
      allocated += size - allocated;
      last_increment = size - allocated; 
      upage += last_increment;
    }else{
      allocated += PGSIZE; 
      last_increment = PGSIZE; 
      upage += last_increment;
    }
    pages++; 
  }

  return pages + 1 ; 
  
}
/*
  Preload pages for read/write syscalls. When preloading pages there are two cases: 
   1. All of buffer pages are already created. But they may be in swap, loaded or in file. 
   2. Pages need to be created with stack growth.
*/
static void 
preload(void *fault_address, size_t size)
{
  ASSERT(is_user_vaddr(fault_address))

  struct thread *cur = thread_current(); 
  struct spage_entry *page = NULL;
  void *buffer = fault_address;
  int i = 0;
  size_t allocated = 0;
  int cant_pages =  calc_pages(fault_address, size);
  while (i != cant_pages)
  {
    size_t page_size = (allocated + PGSIZE > size)? size - allocated: PGSIZE; 
    allocated += page_size; 
    /* If page is already loaded, then continue to the next iteration because there is no point in stack growth or page reclamation */
    if (pagedir_get_page(cur->pagedir, buffer) != NULL){
      buffer += page_size;
      i++; 
      continue; 
    }

    /* Search the page in supplementary page table. If we found the page and is not loaded then load the page from file or swap. If we don't find 
    the page then need to grow the stack. */
    page= lookup_page(cur, pg_round_down(buffer));
    bool success = false;
    if (page != NULL && !page->loaded){
      switch (page->type)
      {
      case EXECUTABLE:
         success = load_file_page(page);
         return;
      case PAGE:
         success = load_page(page);
         break;
      }
    }else if (page == NULL && (cur->esp - 32)  <= buffer){
      success = stack_growth(buffer);
    }else{
      exit(-1);
    }

    if (!success)
      PANIC("No se logro cargar la página.");

    struct frame_entry* fte = lookup_uframe(cur, pg_round_down(buffer)); 
    if (fte == NULL)
      PANIC("No hay memoria para precargar el buffer"); 
    
    fte->pinned = true;

    buffer += page_size;
    i++; 
  }
}