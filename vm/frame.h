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
    void* frame;
    void* upage;

    struct thread* owner; 

    // Save if it is data, file or executable. 
    // Save if it is pinned.

    struct list_elem elem; 
};

void frame_init(void);

void* create_frame(void); 
bool install_frame(void *frame, void* upage, bool writable);
void destroy_frame(void *frame); 
uint32_t* evict_frame(void);


#endif