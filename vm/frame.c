#include "vm/frame.h"

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"

#include "userprog/process.h"
#include "userprog/pagedir.h"


void 
frame_init(void)
{
    list_init(&frame_table);
}


/*
    Creates a new frame and saves it in the frame table. 
    Returns the newly install frame or NULL if allocation of installation failed.
*/
uint32_t*
create_frame(void *upage, bool writable)
{
    uint32_t* frame = (uint32_t*)palloc_get_page(PAL_USER | PAL_ZERO);
    if (frame)
    {
        if (! install_frame(frame, upage, writable)){
            palloc_free_page((void *)  frame);
            return NULL;
        }
        struct frame_entry *new_frame = (frame_entry*) malloc(sizeof(frame_entry));
        
        new_frame->frame = frame; 
        new_frame->owner = thread_current();
        new_frame->upage = upage;
        list_push_back(&frame_table, new_frame->elem);
    }
    return frame
}

bool 
install_frame(uint32_t *frame, void *upage, bool writable)
{
    return install_page(upage, frame, writable); 
}