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
    page->upage = upage; 
    page->loaded = true;
    page->dirty = false; 
    page->accessed = false;
    page->access_time = timer_ticks();

    hash_insert(&cur->sup_table, &page->elem); 

    return true; 
}
