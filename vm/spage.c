#include "devices/timer.h"
#include "string.h"
#include "stdio.h"
#include "hash.h"
#include "list.h"


#include "vm/spage.h"
#include "vm/frame.h"
#include "vm/swap.h"

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
    page->writable = writable; 
    page->type = PAGE; 
    page->file = NULL;
    page->swap_id = -1; 
    page->in_swap = false; 

    hash_insert(&cur->sup_table, &page->elem); 

    return true; 
}


bool get_file_page(struct file *file, off_t ofs, uint32_t read_bytes, uint32_t zero_bytes, bool writable, Page_Type type, void* upage)
{
    struct thread *cur = thread_current(); 
    struct  spage_entry *page; 

    page = (struct spage_entry*) malloc(sizeof(struct spage_entry)); 

    if (!page)
        return false;
    
    page->upage = upage; 
    page->writable = writable;
    page->loaded = false;
    page->type = type;
    page->file = NULL;
    page->swap_id = -1; 
    page->in_swap = false; 


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
    ASSERT(page->type == EXECUTABLE || page->type == MMFILE);
    ASSERT(page->file != NULL); // BRUH este assert me ahorro muchas horas mas haha .
    
    if (page->in_swap)
        return load_page(page);

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
    
    page->loaded = true; 
    return true; 
}

bool load_page(struct spage_entry *page)
{
    ASSERT(page->type == PAGE || (page->type == EXECUTABLE && page->writable));

    /* Create a new frame for this page. If  memory is full, this will evicted a currently installed frame */
    void *kpage = create_frame();
    if (!kpage)
        PANIC("ERROR! RUN OUT OF MEMORY"); 
    /* Install the frame on page directory */
    if (!install_frame(kpage, page->upage, page->writable)){
        destroy_frame(kpage); 
        return false; 
    }
    
    /* Load page content from swap */
    if (page->in_swap)
        swap_deallocate(page->upage, page->swap_id);


    /* Update page variables */
    page->in_swap = false; 
    page->loaded = true; 
    page->swap_id = -1;

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

void remove_SPentry(struct hash *table, void *upage)
{
    struct spage_entry e;
    e.upage = upage; 

    struct hash_elem *page_elem =  hash_delete(table, &e.elem); 
    struct spage_entry *page = hash_entry(page_elem, struct spage_entry, elem);

    if (page->file)
        free(page->file); 

    if (page->in_swap)
        swap_free(page->swap_id);

    free(page);
}