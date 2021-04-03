---
description: >-
  Como se modificaron las structs proveídas por PintOS y otras estructuras que
  se crearon para manejar las funcionalidades de los programas de usuario.
---

# Structs



{% tabs %}
{% tab title="Thread" %}
```c
struct thread
  {
    /*
      MISMOS MIEMBROS QUE EN LA ASIGNACIÓN ANTERIOR.
    */
    tid_t parent;
    tid_t child_waiting;
    bool child_load;
    bool child_status;
    bool children_init;
    struct hash children;
    
    struct semaphore exec_sema; 
    struct lock wait_lock;
    struct condition wait_cond; 

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Used for filesys interaction */
     int fd_next;                             /* ID of fiel descriptor*/
     struct list files;
     int fd_exec;
    
}
```

Se agregaron `parent` y `child_waiting` para que el thread conozca cual es su thread padre y que conozca a que thread hijo está esperando.  `child_load` y `child_status` son variables de estado del thread que el thread padre conozca si el proceso hijo se logró ejecutar o falló al momento de cargarse. 

Se utilizo un **Hash Map** para mantener el control de los procesos hijo que tiene un proceso. Además se tienen un semáforo que permite avisarle al padre cuando se carga un hijo exitosamente o ocurre un error. 

Se usa `fd_next` para llevar un control del siguiente **file descriptor** que pertenezca al thread. Se decidió que cada thread tenga su propia lista de **file descriptors** por separado. Y `fd_exec` tiene el **file descriptor** del archivo ejecutable que utiliza el proceso, y si no es un thread de un proceso su valor default es -1. 
{% endtab %}

{% tab title="Open File" %}
```c
struct open_file{
    int fd;
    char* name;
    struct file *tfiles;
    struct list_elem af;
    struct list_elem at;
  };
```

Esta estructura representa un **file descriptor.** `fd` representa el ID del **file descriptor**, se guarda el nombre del archivo y un puntero al archivo que el **file descriptor** representa. Además se tienen dos `list_elem`  para ingresar el **file descriptor** a la lista del thread y a una lista general.
{% endtab %}

{% tab title="Children\_process" %}
```c
struct children_process{
    struct hash_elem elem;
    int child_id;
    int pid; 
    int status;  
    bool parent_waited;
    bool finish; 
};
```

Esta estructura representa un block de control para los procesos hijos. Esta estructura se ingresa al hash map de hijos del padre. Cuando un proceso hijo termina su ejecución actualiza su status en el hash del padre.
{% endtab %}
{% endtabs %}

