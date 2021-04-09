#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "hash.h"
/*
    Define the status of a child process.
    When the child stops he updates this variables to comunicate 
    to it's parent, the status and that it has finished.
*/
struct children_process{
    struct hash_elem elem;
    int child_id;
    int pid; 
    int status;  
    bool parent_waited;
    bool finish; 
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
// static unsigned children_hash (const  struct hash_elem *elem, void *aux UNUSED); 
// static bool childres_hash_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
#endif /* userprog/process.h */
