#ifndef VM_SPAGE_H
#define VM_SPAGE_H

#include <hash.h>

/*
    The Suplementary Page Entry is a control struct to 
    do bookeeping of all the pages a process P has allocated. 

    This entries are only deleted when process is exiting or if 
    a memory map file is being unmapped. When page is sent to swap, it's 
    SPE is updated. 
*/
struct spage_entry
{
    void* upage;

    bool loaded;
    bool dirty; 
    bool accessed; 

    uint64_t access_time; 
    struct hash_elem elem; 
};

bool get_page(void* upage, bool writable);
void destroy_SPtable(struct hash *table);

#endif