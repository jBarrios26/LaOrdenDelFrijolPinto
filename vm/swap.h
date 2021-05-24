#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "bitmap.h"
#include "devices/block.h"


void swap_init(void); 
void swap_read(void *frame, size_t idx);
void swap_write(void *frame, size_t idx);

size_t swap_allocate(void *frame);
void swap_deallocate(void *frame, size_t idx);
void swap_free(size_t idx); 

#endif