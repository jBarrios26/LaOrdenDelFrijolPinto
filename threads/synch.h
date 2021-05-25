#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* Donations. */
struct donation 
  {
    int priority;           /* Donated priority. */
    struct lock *lock;      /* Lock of the donation. */
    struct list_elem elem;
  };

/* A counting semaphore. */
/* PintOS semaphores allow for n threads to access a critical code section. Not one thread can see the current
   value of the semaphore. Threads can only increment the semaphore's value or decrease it. 
   If the thread that is waked has a higher priority than the current thread, the current thread will give up 
   the processor. */
struct semaphore 
  {
    unsigned value;             /* Current value. */
    struct list waiters;        /* List of waiting threads. */
  };

void sema_init (struct semaphore *, unsigned value);    
/* Increments the semaphore value. If there's threads waiting, wakes the thread with highest priority. */
void sema_up (struct semaphore *);                    
/* Decreases the semaphore value. If the semaphore's value is 0, the semaphore is inserted into a waiting list. */
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_self_test (void);

/* Lock */
/* PintOS uses a binary semaphore to control access and a list with the threads waiting for the lock. 
   Priority implementation is done in the semaphores due to the lock implementation. 
   PintOS has a priority based scheduler that means that if a low priority thread is holder of a lock said 
   thread can cause a dead lock due to its low priority. This is fixed by implementing priority donations. */
struct lock 
  {
    int id;                     /* Used to compare against other locks. */
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore semaphore; /* Binary semaphore controlling access. */
    struct list_elem elem;      /* Threads waiting for the lock to be released. */
    struct donation *donated;   /* This structure has all the donation information of the current thread. */
  };

struct list_elem *find_lock (struct list *donations, struct lock *lock);
int print_donos(struct list *donations);

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);


/* Condition variable. */
/* Monitors have a higher abstraction level than locks or semaphores.
   The execution process of a condition variable is described as follows:
   1. A lock is set over a critical section.
   2. Information is modified.
   3. The condiotion variable is verified, if not satisfied then the thread is put to sleep with the lock held.
   4. Someone else wakes up the thread. 
   5. The lock is released. */ 
struct condition 
  {
    struct list waiters;        /* List of waiting threads. */
  };

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")


//void donate_priority(struct thread *donor, struct lock *lock);
void return_priority(struct lock *lock);
#endif /* threads/synch.h */
