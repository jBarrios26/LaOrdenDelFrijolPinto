---
description: >-
  Esta tabla es la encargada de mantener el control de las páginas físicas que
  se han asignado a los programas de usuario.
---

# Frame Table

## Lo que incluye

{% tabs %}
{% tab title="Frame Table" %}
```c
struct list frame_table;
struct lock lock_frame; 
struct lock evict_lock; 
```

La frame table consiste en una lista con todas las `frame_entry`, un lock para asegurar la sincronización de lectura y escrituras a la lista,  y otro lock para la sincronización del proceso de desalojamiento.
{% endtab %}

{% tab title="Frame Entry" %}
```c
struct frame_entry
{
    void* frame;
    void* upage;

    bool pinned; 
    struct thread* owner; 
    uint64_t accessed_time; 
    // Save if it is data, file or executable. 
    // Save if it is pinned.

    struct list_elem elem; 
};
```

Nuestra estructura para las entradas del frame tiene el frame, la dirección de la memoria de usuario \(`upage`\), a que thread pertenece el frame y si el frame fue pineado. 
{% endtab %}
{% endtabs %}

## Operaciones básicas de la frame table

La frame table tiene muchas funciones a lo largo del proyecto y va a aparecer en muchos archivos. 

### Asignar un nuevo frame

Se debe de pedirle un nuevo frame al kernel con `palloc_get_page`  con las banderas `PAL_USER | PAL_ZERO`  para pedir una página del pool de usuario y que esté inicializada en cero .Si hay páginas disponibles del pool de usuario entonces se debe de crear una nueva `frame_entry`, de lo contrario hay que desalojar un frame y cambiarle el thread owner y la `upage`. 

### Instalar un frame

Instalar un nuevo frame en el directorio de páginas del proceso. Esto marca como válido el rango de direcciones de la nueva página en el directorio del usuario  y va a redirigir todo las traducciones a está página. 

### Buscar un frame de un usuario 

Esta función solo debe de buscar un frame que pertenece a un thread en la lista de frame. 

### Borrar un frame

Libera la memoria de un frame y devuelve el frame al pool de páginas de usuario. 

{% hint style="warning" %}
Esto debe de usar locks para mantener la sincronización.
{% endhint %}

## Desalojo de frames

{% hint style="danger" %}
**MISSION FAILED ! ! !** 
{% endhint %}

