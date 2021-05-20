#include "swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "debug.h"
#include "stdio.h"

#define BLOCKS_PER_PAGE PGSIZE / BLOCK_SECTOR_SIZE

struct bitmap *swap_table; 
struct block *global_swap_block; 


/*
    Initialize the swap table. The swap table consist in a bitmap that keeps control of 
    which swap sectors/blocks are in use.
*/
void 
swap_init(void)
{
    global_swap_block = block_get_role(BLOCK_SWAP);
    if (!global_swap_block)
         PANIC("ERROR! CANNOT CREATE SWAP FILE");

    swap_table = bitmap_create(block_size(global_swap_block));
    if (!swap_table)
        PANIC("ERROR! CANNOT CREATE SWAP TABLE");

}


/* Reads the sector with index idx from the swap file and saves it in frame. */
void 
swap_read(void *frame, size_t idx)
{
    for (int i = 0; i < 8; i++)
        block_read(global_swap_block, idx + i, frame + (i * BLOCK_SECTOR_SIZE)); 
}

/* Writes the content of frame to the sector with index idx of the swap file  */
void 
swap_write(void *frame, size_t idx)
{
    
    for (int i = 0; i < 8; i++)
        block_write(global_swap_block, idx + i, frame + (i * BLOCK_SECTOR_SIZE)); 
}


/* Allocates the frame content in the swap file, returns the index of the first sector the page is using in the swap.  */
size_t 
swap_allocate(void *frame)
{
    size_t idx = bitmap_scan_and_flip(swap_table, 0, BLOCKS_PER_PAGE, false);
    swap_write(frame, idx); 
    return idx;
}

/* Reads a page from swap file and free its sectors. */
void 
swap_deallocate(void *frame, size_t idx)
{
    bitmap_scan_and_flip(swap_table, idx, BLOCKS_PER_PAGE, true);
    swap_read(frame, idx); 
}


