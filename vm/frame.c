#include "vm/frame.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"

#include "userprog/process.h"
#include "userprog/pagedir.h"

struct frame_entry *lookup_frame(void *frame); 

void 
frame_init()
{
    list_init(&frame_table);
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
        list_push_back(&frame_table, &new_frame->elem);
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
        list_remove(&fte->elem); 
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
    struct list_elem *iter = list_begin(&frame_table); 
    while (iter != list_end(&frame_table)){
        struct frame_entry *fte = list_entry(iter, struct frame_entry, elem); 
        if (fte->frame == frame)
            return fte; 
        iter = list_next(iter); 
    }
    return NULL; 
}