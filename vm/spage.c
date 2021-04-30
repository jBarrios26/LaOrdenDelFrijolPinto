#include "devices/timer.h"
#include "string.h"
#include "stdio.h"
#include "hash.h"
#include "list.h"


#include "vm/spage.h"
#include "vm/frame.h"

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

#include "userprog/process.h"
#include "userprog/pagedir.h"

#include "filesys/file.h"
#include "filesys/filesys.h"


/*
    Creates a new Supplemantary Page Entry and insert it to the Supplementary Page Table. 
    Expects that upage it's already associated to a installed and loaded frame. 
*/ 
bool 
get_page(void *upage, bool writable)
{
    struct thread *cur = thread_current(); 
    struct spage_entry *page;

    page = (struct spage_entry*) malloc(sizeof(struct spage_entry));

    /* */
    if (!page)
        return false;
    
    page->upage = upage; 
    page->loaded = true;
    page->dirty = false; 
    page->type = PAGE; 
    page->accessed = false;
    page->file = NULL;
    page->access_time = timer_ticks();

    hash_insert(&cur->sup_table, &page->elem); 

    return true; 
}


bool get_file_page(struct file *file, off_t ofs, uint32_t read_bytes, uint32_t zero_bytes, bool writable, void* upage)
{
    struct thread *cur = thread_current(); 
    struct  spage_entry *page; 

    page = (struct spage_entry*) malloc(sizeof(struct spage_entry)); 

    if (!page)
        return false;
    
    page->upage = upage; 
    page->loaded = false;
    page->dirty = false; 
    page->type = EXECUTABLE;
    page->accessed = false;
    page->file = NULL;
    page->access_time = timer_ticks();

    struct file_page *file_entry = (struct file_page*) malloc(sizeof(struct file_page));
    if (!file_entry)
        return false; 
    page->file = file_entry;
    file_entry->file = file; 
    file_entry->ofs = ofs;
    file_entry->upage= upage; 
    file_entry->writable = writable; 
    file_entry->read_bytes = read_bytes;
    file_entry->zero_bytes = zero_bytes; 

    hash_insert(&cur->sup_table, &page->elem); 
    return true;
}

bool load_file_page(struct spage_entry *page)
{
    ASSERT(page->type == EXECUTABLE);
    ASSERT(page->file != NULL); // BRUH este assert me ahorro muchas horas mas haha .

    struct file_page *file_ = page->file;

    void *kpage = create_frame();
    if (kpage == NULL)
        return false;

    /* Place file pos pointer in the executable address we are about to read. */
    file_seek(file_->file, file_->ofs);

    /* Load this page. */
    if (file_read (file_->file, kpage, file_->read_bytes) != (int) file_->read_bytes)
    {
        destroy_frame(kpage); 
        return false;
    }
    memset (kpage + file_->read_bytes, 0, file_->zero_bytes);

    /* Add the page to the process's address space. */
    if (!install_frame(kpage, page->upage, file_->writable))
    {
        destroy_frame(kpage); 
        return false;
    }
    
    page->loaded = false; 
    page->access_time = timer_ticks();
    return true; 
}

struct spage_entry *lookup_page(struct thread *owner ,void *upage){

    struct spage_entry page;
    page.upage = upage; 
    struct hash_elem *spage_elem = hash_find(&owner->sup_table, &page.elem);

    if (spage_elem == NULL)
        return NULL;
    return hash_entry(spage_elem, struct spage_entry, elem);
}