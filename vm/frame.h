#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <debug.h>
#include <list.h>
#include <hash.h>
#include "threads/synch.h"
#include <stdint.h>

struct list frame_table; 
struct lock lock_frame; 
struct lock evict_lock;

/*
    The frame table contains the frame entries that all of the processes 
    are holding. Save the frame pointer, which is a kernel page, and saves the user page, the page that 
    the user sees. 

*/
struct frame_entry
{
    void* frame;
    void* upage;

    bool pinned; 
    struct thread* owner; 
    uint64_t accessed_time; 
    // Save if it is data, file or executable. 
    // Save if it is pinned.

    struct list_elem elem; 
};

void frame_init(void);

void* create_frame(void); 
bool install_frame(void *frame, void* upage, bool writable);
void destroy_frame(void *frame); 
struct frame_entry* evict_frame(void);
struct frame_entry* lookup_uframe(struct thread *t, void *upage); 
void unpin_frames(struct thread *t);

#endif