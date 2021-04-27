#include "devices/timer.h"
#include "hash.h"
#include "list.h"


#include "vm/spage.h"
#include "vm/frame.h"

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

#include "userprog/process.h"
#include "userprog/pagedir.h"


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
    page->loaded = true;
    page->dirty = false; 
    page->type = EXECUTABLE;
    page->accessed = false;
    page->file = NULL;
    page->access_time = timer_ticks();

    struct file_page *file_entry = (struct file_page*) malloc(sizeof(struct file_page));
    if (!file_entry)
        return false; 

    file_entry->file = file; 
    file_entry->ofs = ofs;
    file_entry->upage= upage; 
    file_entry->writable = writable; 
    file_entry->read_bytes = read_bytes;
    file_entry->zero_bytes = zero_bytes; 

    hash_insert(&cur->sup_table, &page->elem); 
    return true;
}

