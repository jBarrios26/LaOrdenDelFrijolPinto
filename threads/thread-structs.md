---
description: Las estructuras que creamos y las estructuras que modificamos.
---

# Structs



{% tabs %}
{% tab title="thread" %}
```c
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                 /* Priority. */
    int nice;
    struct list_elem allelem;     /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
    int64_t time_sleeping;              /* Tiempo que duerme un thread*/

    /* Synchonization variables */
    struct lock *waiting; 
    struct thread *lock_holder;
    struct list locks; 
    struct list donations; 
    int original_priority;
}
```

Se agrego `time_sleeping` que es el tiempo que un thread va a dormir. `Donations` es una lista con las donaciones que un thread recibe, `waiting` es el lock que un thread esta esperando, `lock_holder` es el thread que tiene el lock que el thread está esperando y `original_priority` es la prioridad original que tiene el thread. 
{% endtab %}
{% endtabs %}



