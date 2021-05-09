#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "bitmap.h"
#include "devices/block.h"

struct bitmap swap_table; 
static struct block* global_swap_block; 

void swap_init(void); 
bool swap_read(void *frame, size_t idx);
bool swap_write(void *frame, size_t idx);

bool swap_allocate(void *frame);
bool swap_deallocate(void *frame);

#endif