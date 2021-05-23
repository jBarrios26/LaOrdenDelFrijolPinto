#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/spage.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#include "userprog/process.h"
#include "userprog/pagedir.h"

#include "devices/timer.h"
#include "stdio.h"
#include "debug.h"
#include "string.h"


struct frame_entry *lookup_eviction_victim(void);
struct frame_entry *lookup_frame(void *frame); 


void 
frame_init()
{
    list_init(&frame_table);
    lock_init(&lock_frame);
    lock_init(&evict_lock);
}


/*
    Creates a new frame and saves it in the frame table. 
    Returns the new frame or NULL if allocation failed.
*/
void*
create_frame()
{
    void* frame = palloc_get_page(PAL_USER | PAL_ZERO);
    if (frame)
    {
        struct frame_entry *new_frame = (struct frame_entry*) malloc(sizeof(struct frame_entry));
        
        new_frame->frame = frame; 
        new_frame->owner = thread_current();
        new_frame->accessed_time = timer_ticks();
        new_frame->pinned = false; 

        lock_acquire(&lock_frame);
            list_push_back(&frame_table, &new_frame->elem);
        lock_release(&lock_frame);
    }else { 
        struct frame_entry *new_frame = evict_frame(); 
        new_frame->owner = thread_current();
        new_frame->accessed_time = timer_ticks(); 
        new_frame->pinned = false; 
        frame = new_frame->frame;
        
        lock_acquire(&lock_frame);
            list_push_back(&frame_table, &new_frame->elem);
        lock_release(&lock_frame);

    }
    return frame;
}

/*
    Install a frame in the process page directory. 
*/
bool 
install_frame(void *frame, void *upage, bool writable)
{
    struct frame_entry *fte = lookup_frame(frame); 
    bool success = install_page(upage, frame, writable); 
    if (success)
        fte->upage = upage; 
    return success;
}

/*
    Destroys a frame table entry. Releases all allocated resources. 
*/
void
destroy_frame(void *frame)
{
    struct frame_entry *fte = lookup_frame(frame); 
    if (fte)
    { 
        lock_acquire(&lock_frame);
            list_remove(&fte->elem); 
        lock_release(&lock_frame);
        palloc_free_page(fte->frame); 
        free(fte); 
    }
}
/*
    Searchs up a frame entry in the frame table. Return the frame table entry or NULL if it is not allocated. 
*/
struct frame_entry 
*lookup_frame(void *frame)
{
    lock_acquire(&lock_frame);
    struct list_elem *iter = list_begin(&frame_table); 
    while (iter != list_end(&frame_table)){
        struct frame_entry *fte = list_entry(iter, struct frame_entry, elem); 
        if (fte->frame == frame){
            lock_release(&lock_frame); 
            return fte; 
        }
        iter = list_next(iter); 
    }
    lock_release(&lock_frame); 
    return NULL; 
}


struct frame_entry *evict_frame(void)
{
    struct frame_entry *frame;
    lock_acquire(&evict_lock);
        frame = lookup_eviction_victim(); 
        if (!frame)
            PANIC("ERROR! NO FRAME TO EVICT");
        list_remove(&frame->elem); 
    lock_release(&evict_lock);

    struct spage_entry *page = lookup_page(frame->owner, frame->upage);
    size_t idx = -1; 
    bool in_swap = false;
    if (pagedir_is_dirty(frame->owner->pagedir, page->upage)){
        idx = swap_allocate(frame->upage);
        in_swap = true;
    }

    memset(frame->frame, 0, PGSIZE);

    page->swap_id = idx; 
    page->in_swap = in_swap; 
    page->loaded = false;

    pagedir_clear_page(frame->owner->pagedir, frame->upage);
    return frame;
}

/*  Searches a frame to evict. Chooses the frame with the lowest accessed_time (based on LRU) */
struct frame_entry*
lookup_eviction_victim(void)
{
    struct frame_entry *victim = NULL; 
    uint64_t ticks = timer_ticks();

    struct list_elem *iter = list_begin(&frame_table); 
    for (; iter != list_end(&frame_table); iter = list_next(iter))
    {
        struct frame_entry *candidate = list_entry(iter, struct frame_entry, elem);
        if (!victim){
            victim = candidate; 
            continue;
        }

        if (!candidate->pinned &&  (ticks - victim->accessed_time) < (ticks -  candidate->accessed_time))
            victim = candidate;
    }

    return victim;
}

/*
    Searches the frame in frame table that it is own by thread t and has user address upage. 
*/
struct frame_entry 
*lookup_uframe(struct thread *t ,void *upage)
{
    lock_acquire(&lock_frame);
    struct list_elem *iter = list_begin(&frame_table); 
    while (iter != list_end(&frame_table)){
        struct frame_entry *fte = list_entry(iter, struct frame_entry, elem); 
        if (fte->owner == t && fte->upage == upage){
            lock_release(&lock_frame); 
            return fte; 
        }
        iter = list_next(iter); 
    }
    lock_release(&lock_frame); 
    return NULL; 
}


/*
    Unpins all the pinned frames of thread t.
*/
void 
unpin_frames(struct thread *t)
{
    lock_acquire(&lock_frame);
    struct list_elem *iter = list_begin(&frame_table); 
    while (iter != list_end(&frame_table)){
        struct frame_entry *fte = list_entry(iter, struct frame_entry, elem); 
        if (fte->owner == t && fte->pinned){
            fte->pinned = false;
        }
        iter = list_next(iter); 
    }
    lock_release(&lock_frame); 

}