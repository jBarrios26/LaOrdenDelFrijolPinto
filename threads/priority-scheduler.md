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
* **get\_next\_thread**

### ¿Cómo ordenar las listas de pintOS?

{% tabs %}
{% tab title="Insert ordered" %}
```c
list_insert_ordered(&ready_list, priority_value_less, NULL)
```

{% hint style="info" %}
Ordena la lista de acuerdo a la función priority value less. 
{% endhint %}
{% endtab %}

{% tab title="Priority Value Less" %}
```c
int 
priority_value_less(const struct list_elem *a_, const struct list_elem *b_, void *aux)
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



