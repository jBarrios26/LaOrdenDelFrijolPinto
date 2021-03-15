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
    int pid;
    int status;  
    bool finish; 
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
