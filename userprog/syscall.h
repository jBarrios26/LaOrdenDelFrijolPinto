#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
typedef int pid_t;
typedef mapid_t;

// User Program
void syscall_init (void);
struct open_file * get_file(int fd);
void exit(int status);
pid_t exec(const char* cmd_line);
bool create(const char* file, unsigned initial_size);
bool remove(const char* file);
int open(const char* file);
int filesize(int fd);
int read(int fd, char* buffer, unsigned size);
int write (int fd, void* buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell (int fd);
void close(int fd);

// Virtual Memory
mapid_t mmap (int fd, void *addr);
void munmap (mapid_t mapping);

#endif /* userprog/syscall.h */
