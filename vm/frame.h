#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <debug.h>
#include <list.h>
#include <hash.h>
#include "threads/synch.h"
#include <stdint.h>

struct list frame_table; 
struct lock frame_lock; 

/*
    The frame table contains the frame entries that all of the processes 
    are holding. Save the frame pointer, which is a kernel page, and saves the user page, the page that 
    the user sees. 

*/
struct frame_entry
{
    uint32_t* frame;
    void* upage;

    struct thread* owner; 

    // Save if it is data, file or executable. 
    // Save if it is pinned.

    struct list_elem elem; 
}

void frame_init(void);

uint32_t* create_frame(void *upage, bool writable); 
bool install_frame(uint32_t* frame, void* upage, bool writable);
bool destroy_frame(struct frame_entry *fte); 
uint32_t* evict_frame();


#endif