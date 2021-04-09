  /* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "malloc.h"
static int ids = 0;
bool donations_value_less(const struct list_elem* a, const struct list_elem* b, void* aux UNUSED);
bool sema_value_less(const struct list_elem* a, const struct list_elem* b, void* aux UNUSED);
int search_lock_donated_priority (struct list *donations, struct lock *lock);
/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  
  while (sema->value == 0) 
    {
      list_insert_ordered(&sema->waiters, &thread_current ()->elem, priority_value_less, NULL);
      thread_block ();
    }
  sema->value--;

  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  
  struct thread *next_thread = NULL;
  struct thread *cur = thread_current();
  if (!list_empty (&sema->waiters)) 
  {
    list_sort (&sema->waiters, priority_value_less, NULL);

    next_thread = list_entry (list_pop_back (&sema->waiters), struct thread, elem);
    thread_unblock (next_thread);
  }
  sema->value++;
  
  if (next_thread != NULL && !intr_context() && cur->priority < next_thread->priority){
    thread_yield();
  }
  
  intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);
  lock->id = ids++;
  lock->holder = NULL;
  lock->donated = NULL;
  sema_init (&lock->semaphore, 1);
}

struct list_elem *find_lock (struct list *donations, struct lock *lock)
{
  struct list_elem *iter = list_begin(donations);
  while (iter != list_end (donations))
  {
    struct donation *d = list_entry(iter, struct donation, elem);
    if (d->lock == lock) return iter;
    iter = list_next(iter);
  }
  return NULL;
  
}



/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  int cont = 0;
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));


  thread_current ()->lock_holder = lock->holder;

  enum intr_level old_level;

  old_level = intr_disable ();
  struct thread *cur = thread_current ();
  struct thread *holder = lock->holder;
  struct lock *lock_holder = lock;
   
  while(holder != NULL && cont < 8){
    if (holder->priority < cur->priority)
    {
      struct list_elem *has_donation = find_lock (&holder->donations, lock_holder);
      if (has_donation == NULL)
      {
        struct donation *donor = malloc (sizeof (struct donation));
        donor->priority = cur->priority;
        donor->lock = lock_holder;
        list_insert_ordered (&holder->donations, &donor->elem,donations_value_less, NULL);
        holder->priority = donor->priority;
        lock_holder->donated = donor;
      }else {
        struct donation *previous = list_entry (has_donation, struct donation, elem);
        if (previous->priority < cur->priority)
        {
          holder->priority = cur->priority;
          previous->priority = cur->priority;
          list_sort(&holder->donations,donations_value_less,NULL);
        }
      }
    }
    if(holder->waiting != NULL){
      lock_holder = holder->waiting;
       holder = holder->waiting->holder;
    }else{
      holder = NULL;
      lock_holder = NULL;
    }
    cont++;
  }
  cur->waiting = lock;
  /* if (holder != NULL){
    if (holder->priority < cur->priority)
    {
      holder->priority = cur->priority;
    }
    cur->waiting = lock;
  } */
  sema_down (&lock->semaphore);
  lock->holder = thread_current ();
  list_push_back(&cur->locks, &lock->elem);
  cur->waiting = NULL;
  intr_set_level (old_level);
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  enum intr_level old_level;

  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  old_level = intr_disable ();

  lock->holder = NULL;
  struct thread *cur = thread_current ();
  if (lock->donated != NULL)
  {
    list_remove (&lock->donated->elem);
    if (list_empty (&cur->donations))
    {
      cur->priority = cur->original_priority;
      lock->donated = NULL;
    }else {
      struct donation *donations = list_entry (list_back (&cur->donations), struct donation, elem);
      cur->priority = donations->priority;
      lock->donated = donations;
    }
  }
  /* if (list_empty (&cur->locks)){
  list_remove (&lock->elem);
    cur->priority = cur->original_priority;
  }else {
    list_sort (&cur->locks, sort_locks, NULL);
    struct lock *lock2 = list_entry (list_pop_back (&cur->locks), struct lock, elem);
    cur->priority = lock2->holder->priority;
  } */

  list_remove(&lock->elem);

  sema_up (&lock->semaphore);
  intr_set_level (old_level);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
 	list_push_back (&cond->waiters, &waiter.elem);
    //list_push_front (&cond->waiters, &waiter.elem);
 // list_insert_ordered(&cond->waiters, &waiter.elem, priority_value_less, NULL);
	
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
	ASSERT (cond != NULL);
  	ASSERT (lock != NULL);
  	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	void * aux = NULL;
	// if (is_sorted(list_front(&cond->waiters), list_back(&cond->waiters), priority_value_less, aux))
	// 	printf("is sorted");
	
	if (!list_empty (&cond->waiters)) {
		// ordenar lista primero

		list_sort (&cond->waiters, sema_value_less, aux);
		
		// list_reverse(&cond->waiters);

		sema_up (&list_entry (list_pop_back (&cond->waiters), struct semaphore_elem, elem)->semaphore);
	}
}


/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);

}


/*
  Donor thread shares its priority to the thead that is holding the lock.
  If another high priority thread has already donated to lock holder, the highest priority stays. 

  Donor threads also donates its priority to locks that lock holder it's waiting
void 
donate_priority(struct thread *donor, struct lock *lock)
{
  struct thread *lock_holder = lock->holder;
  
  if (lock_holder == NULL)
    return;

  if (lock_holder->donated_priority == 0 && lock_holder->priority < donor->priority )
  {
    lock_holder->original_priority = lock_holder->priority;
    lock_holder->priority = donor->priority;
    lock_holder->donated_priority = donor->priority;
  }
}
*/

/* 
  Returns to the original priority.
void 
return_priority(struct lock *lock)
{
  struct thread *lock_holder =lock->holder;

  lock_holder->priority = lock_holder->original_priority;
  lock_holder->original_priority = 64;
  lock_holder->donated_priority = 64;

}
*/


bool 
donations_value_less(const struct list_elem* a, const struct list_elem* b, void* aux UNUSED)
{
  const int a_member = (list_entry(a, struct thread, elem))->priority;
  const int b_member = (list_entry(b, struct thread, elem))->priority;
  return a_member < b_member;
}

bool 
sema_value_less(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED)
{
  struct semaphore_elem *a = list_entry (a_, struct semaphore_elem, elem);
  struct semaphore_elem *b = list_entry (b_, struct semaphore_elem, elem);

  const struct thread *t1 = list_entry (list_front(&a->semaphore.waiters), struct thread, elem);
  const struct thread *t2 = list_entry (list_front(&b->semaphore.waiters), struct thread, elem);

  return t1->priority <= t2->priority;
}
