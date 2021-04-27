#ifndef VM_SPAGE_H
#define VM_SPAGE_H

#include <hash.h>
#include "filesys/off_t.h"
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

    enum Page_Type type; 

    struct file_page file;

    uint64_t access_time; 
    struct hash_elem elem; 
};
/*
    Save all the data related to the process executable file. This data is needed to 
    implement lazy loading, this way we can take this data and used it to load it is needed. 
*/
struct file_page
{
    struct file *file; 
    off_t ofs; 
    uint8_t *upage; 
    uint32_t read_bytes; 
    uint32_t zero_bytes; 
    bool writable;
};


/* All the types of data a paga can hold. 
    - PAGE: regular memory page. (can be saved to swap)
    - EXECUTABLE: Memory need for executable file (cannnot be saved to swap, use the file)
 */
enum Page_Type{
    PAGE
    EXECUTABLE;
}

bool get_page(void* upage, bool writable);
void destroy_SPtable(struct hash *table);

#endif