---
description: Esta es la segunda asignación del proyecto de Threads.
---

# Priority Scheduler ⌚

## ¿En que consiste?

El calendarizador que tiene el PintOS por default es un calendarizador Round Robin, entonces la tarea de esta asignación es convertir ese calendarizador Round Robin a un calendarizador basado en prioridades. 

Este cambio de calendarizador no solo afecta como se decide cual es el siguiente thread si no también afecta temas de sincronización. 

## Priority Scheduler

En pintOs, los threads que están listos para ser calendarizados se encuentran en la **ready\_list.** Entonces la manera en la que se implementó el Priority Scheduler fue insertar de manera ordenada por la prioridad los threads a la **ready\_list,** para se debe de analizar el ciclo de vida de un thread y en que momento ingresan a la **ready\_list.** 

### Ciclo de vida del thread

1. El thread empieza en la función **create\_thread**
2. El thread se inicializa e inmediatamente se bloquea, entonces entra a **block\_list**.
3. Cuando se termina de inicializar se desbloquea y pasa por primera vez a **ready\_list.**
4. Cuando se calendariza pasa de estado **READY** a estado **RUNNING**.
5. Cuando ocurre el _timer interrupt pasa de **RUNNING** a **READY,** vuelve a la **ready\_list.**_
6. _Si el thread cede el procesador puede volver a la **ready\_list.**_
7. Finalmente, el thread termina y sale de todas las colas.

Al analizar el ciclo de vida se puede notar que solo hay tres instancias en que un thread pasa a la **ready\_list:**

* **thread\_yield**
* **thread\_unblock**

Los threads luego son desencolados en la función **`next_thread_thread()`** donde se hace un **`pop_back(list)`** a la lista, ya que los threads están ordenados de menor a mayor prioridad.

### ¿Cómo ordenar las listas de pintOS?

{% tabs %}
{% tab title="Insert ordered" %}
```c
list_insert_ordered(&ready_list, &thread->elem, priority_value_less, NULL)
```

{% hint style="info" %}
Ordena la lista de acuerdo a la función priority value less. 
{% endhint %}

{% hint style="warning" %}
La lista está ordenada de menor a mayor prioridad, si las prioridades son iguales conserva el orden de llegada.
{% endhint %}
{% endtab %}

{% tab title="Priority Value Less" %}
```c
int 
priority_value_less(const struct list_elem *a_,
                    const struct list_elem *b_,
                   void *aux)
{
  const struct thread *a = list_entry (a_, struct thread, elem);
  const struct thread *b = list_entry (b_, struct thread, elem);
  return a->priority <= b->priority;
}

```

{% hint style="info" %}
Esta función compara dos elementos de la lista. Tomar en cuenta si se utiliza menor o, menor o igual, porque puede afectar en como ordena la lista.
{% endhint %}
{% endtab %}
{% endtabs %}

### Thread Yield

En la función **thread\_yield\(\)** se llama cuando el thread quiere ceder su tiempo en el procesador. Cuando cede su tiempo en el procesador va a regresar inmediatamente a la lista **ready\_list.** 

### **Thread Unblock**

Cuando un thread regresa del estado **BLOCK** a estado **READY** llama a esta función que lo ingresa de nuevo a la cola **ready\_list.** Es importante notar que todos los threads cuando se están creando empiezan en el estado **BLOCK** y luego se desbloquean con **thread\_unblock\(thread\).** 

## Priority Scheduler: Sincronización

PintOS tiene tres formas de sincronizar porciones de código. Estas estructuras de sincronización también se debe de ser programadas de acuerdo a su prioridad. 

* Locks
* Semáforos
* Monitores o variables de condición

### Locks 

Los locks son una herramienta para sincronizar segmentos de código. El thread para acceder a cierto segmento de código debe de pedir el **lock** y si el kernel no se lo da, debe de ingresar a un cola de espera. 

```c
struct lock 
  {
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore semaphore; /* Binary semaphore controlling access. */
    struct list_elem elem;
  };
```

En PintOS, los locks se implementan con un semáforo binario y se utiliza la lista de espera del semáforo para guardar los threads que estén esperando el **lock**. Por está razón los cambios para tomar en cuenta la prioridad se implementaron en los semáforos.

Como el calendarizador toma en cuenta la prioridad puede pasar que un thread con baja prioridad obtenga el **lock** y nunca vuelva a ser programado por su baja prioridad, esto es una causa de **deadlock**, pero se puede solucionar al implementar las [donaciones de prioridad.](priority-scheduler.md#donacion-de-prioridades)

### Semáforo

Los semáforos se utilizan para permitir a _n_ cantidad de threads acceder a una sección crítica de código. Se inicia con un valor _n_ que sea positivo que indica cuantos threads pueden acceder y ningún thread puede ver el valor del semáforo. Tiene dos métodos principales para modificar el semáforo.

```c
struct semaphore 
  {
    unsigned value;             /* Current value. */
    struct list waiters;        /* List of waiting threads. */
  };
```

* `sema_up() o V():` Incrementa el valor del semáforo, y si hay threads esperando despierta al que tiene la mayor prioridad.
* `sema_down() o P()`:  Disminuye el valor del semáforo, y si el valor del semáforo es 0 lo encola a la lista de espera ordenada por prioridad.

{% tabs %}
{% tab title="sema\_down" %}
```c
void
sema_down(struct semaphore *sema)
{
    ... Verificaciones
    while (sema->value == 0)
    {
        list_insert_ordered(&sema->waiters, 
                            &thread_current()->elem, 
                            priority_value_less, 
                            NULL);
        thread_block();
    }
    sema->value--;
}
```

{% hint style="info" %}
Se utiliza la función [priority value less ](priority-scheduler.md#como-ordenar-las-listas-de-pintos)es la misma que se define para insertar por prioridades a la **ready\_list**. 
{% endhint %}
{% endtab %}

{% tab title="sema\_up" %}
```c
void
sema_up(struct semaphore *sema)
{
    ... Verificaciones
   if (!list_empty (&sema->waiter))
   {
      list_sort (&sema->waiters, priority_value_less, NULL);

       next_thread = list_entry (list_pop_back (&sema->waiters), 
                                  struct thread, 
                                  elem);
       thread_unblock (next_thread);       
   }
   sema->value--;
}
```

{% hint style="info" %}
Se debe de hacer sort antes de sacar un thread de la lista de espera porque la prioridad pudo cambiar en caso de una donación de prioridad.
{% endhint %}
{% endtab %}
{% endtabs %}

{% hint style="warning" %}
Si el thread que salió de la lista de espera del semáforo tiene una prioridad mayor que el thread actual, el thread actual cede el procesador.
{% endhint %}

### Monitores

Los monitores son una herramienta de sincronización con un grado de abstracción mayor que el de los semáforos y los locks. Se necesita tener un lock y una o más variables de condición, el flujo del uso del monitor es el siguiente: 

1. Se toma un lock sobre una sección crítica.
2. Se modifica información.
3. Si no se cumple una condición, por ejemplo, "No se ha recibido la data del usuario", se puede esperar o "dormir" con el lock. 
4. Entidad externa desbloquea el thread de la lista de espera. 
5. Se suelta el lock.

Se debe de tomar en cuenta la prioridad al momento de sacar un thread de la lista de espera.

{% tabs %}
{% tab title="signal" %}
```c
void
signal(struct condition *cond, struct lock *lock)
{
    . . . Verificaciones
    if (!list_empty(&cond->waiters))
    {
				list_sort (&cond->waiters, sema_value_less, aux);
        sema_up (&list_entry (list_pop_back (&cond->waiters), 
                                struct semaphore_elem, elem)->semaphore);
    }
}

```

{% hint style="info" %}
Se utiliza la función [priority value less ](priority-scheduler.md#como-ordenar-las-listas-de-pintos)es la misma que se define para insertar por prioridades a la **ready\_list**. 
{% endhint %}
{% endtab %}
{% endtabs %}

## Donación de Prioridades

Cuando se toma en cuenta las prioridades para programar cada thread puede causar que un thread con baja prioridad tome un lock, luego entre un thread de prioridad más alta que intente tomar el lock pero como el thread de baja prioridad lo tiene va a entrar a la lista de espera del lock.  Si hay varios threads esperando que el thread de baja prioridad libere el lock puede causar deadlocks y que todos tomen más tiempo en ejecutarse. 

Para solucionar esto se realizó la donación de prioridades.

### Cambios a la struct lock, struct thread y una nueva struct.

{% tabs %}
{% tab title="struct lock" %}
```c
struct lock 
  {
    int id;
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore semaphore; /* Binary semaphore controlling access. */
    struct donation *donated;
    struct list_elem elem;
  };
```

Se le agrego un id para poder compararlo con otros locks, una struct donation donde está la información de la donación que tiene actualmente el thread y un list\_elem para ingresar el lock a una lista.
{% endtab %}

{% tab title="struct donation" %}
```c
struct donation 
  {
    int priority;
    struct lock *lock;
    struct list_elem elem;
  };
```

Es una estructura que guarda la información de las donaciones de cada thread. Incluye la prioridad donada, el lock que pertenece la donación y un list\_elem para guardarlo en una lista.
{% endtab %}

{% tab title="struct thread" %}
```c
struct thread
{
    ... Otros miembros
    struct lock *waiting; 
    struct thread *lock_holder;
    struct list locks; 
    struct list donations; 
    int original_priority;
}
```

Lock waiting es el lock que un thread está esperando, list donations es una lista de las donaciones que el thread ha recibido, original_priority es la prioridad original que el thread tiene \(la donación afecta a priority\) y lock\_holder es el thread que tiene el lock._ 
{% endtab %}
{% endtabs %}

### Lock Acquire

En la función lock\_acquire se realizó la implementación de las donaciones de prioridad. Se debe de tomar en cuenta 4 casos:

* Donación simple: Un thread _A_ le dona su prioridad al thread _B_
* Donación múltiple: Un thread _A_ le dona su prioridad al thread _B,_ y también el thread _C_ le dona su prioridad al thread _B._
* Donación anidadas : Un thread _A_ tiene el lock 1, el thread _B_ tiene el lock 2 y está esperando por el lock 1 del thread _A_ y luego el thread _C_ pide el lock 2 del thread _B_.  Entonces el thread C le dona su prioridad
* Donaciones en cadena: Es una forma de donaciones anidadas con una recursión de 8 niveles. 

#### Caso \#1: Donación Simple

```c
lock_acquire(lock):
    IF (current.priority > lock_holder.priority) THEN
        crear struct donation
        lock->holder = current
        insertar donation a la cola de donaciones del thread
        lock_holder.priority = current.priority
    current.waiting = lock
    sema_down()
    lock->holder = current
    current.waiting  = NULL
```

Para implementar las donaciones simples se hace el pseudocódigo anterior. En el se hace la donación si y solo si, la prioridad del thread actual o donante es mayor que el thread que tiene al lock. 

#### Caso \#2: Donación Múltiple

```c
lock_acquire(lock):
    IF (current.priority > lock_holder.priority) THEN
        IF (lock_holder has donation) THEN
            get lock_holder donation
            IF (donation.priority < current.donation) THEN
                swap priorities
                ordenar la lista de donaciones del lock holder
            ELSE
                crear struct donation
                lock->holder = current
                insertar donation a la cola de donaciones del thread
                lock_holder.priority = current.priority
    current.waiting = lock
    sema_down()
    lock->holder = current
    current.waiting  = NULL
```

Para la donación múltiple si se cumple la condición que el `current` tiene una prioridad menor que el `lock_holder` entonces se debe de revisar si el `lock_holder` ya le donaron una vez, de ser así se debe de revisar si la donación previa tiene una prioridad mayor que el de `current` y si eso se cumple hay que cambiar la prioridad de la donación. 

En el caso que no tenga donaciones se realiza la donación simple.

#### Caso \#3 y \#4: Donación anidada y en cadena

```c
lock_acquiere(lock):
    holder = lock.holder
    cont = 0
    WHILE (holder != NULL && cont < 8):
        IF (current.priority > lock_holder.priority) THEN
            IF (lock_holder has donation) THEN
                get lock_holder donation
                IF (donation.priority < current.donation) THEN
                    swap priorities
                    ordenar la lista de donaciones del lock holder
                ELSE
                    crear struct donation
                    lock->holder = current
                    insertar donation a la cola de donaciones del thread
                    lock_holder.priority = current.priority
        holder = lock_holder.waiting
        cont++
    current.waiting = lock
    sema_down()
    lock->holder = current
    current.waiting  = NULL    
```

Para las donaciones anidadas se implemento con un ciclo. Al final de realizar una donación se cambia el `lock_holder` por el lock que está esperando el `lock_holder.`  Las donaciones anidadas se van a realizar 8 veces. 

### Lock Release

Cuando un thread libera el lock, si otro thread le donó su prioridad se debe de liberar la prioridad donada y cambiar a la prioridad original del thread. 

```c
lock_release(lock):
    IF lock has donation THEN
        Remover la donacion de la lista de donaciones
        IF No hay más donaciones THEN
            Regresar a la prioridad original con original_priority
        ELSE
            Obtener la donación con la prioridad más alta
            cambiar la prioridad previa por la nueva.
    sema_up()
```

## ¿Cómo afectan la donación de prioridades al Priority Scheduler?

### SET PRIORITY

Los threads pueden cambiar su prioridad en cualquier momento de su ejecución, entonces si tienen una donación al momento de cambiar su prioridad no deben de perder la prioridad donada.  La prioridad debe de cambiar hasta que regresen la donación. 

{% hint style="info" %}
La struct thread tiene el miembro `original_priority` para solucionar este problema. 
{% endhint %}

### SEMA UP

Hay que ordenar la lista de espera antes de despertar a un thread porque la donación puede cambiar la prioridad de un thread que este en la lista de espera

