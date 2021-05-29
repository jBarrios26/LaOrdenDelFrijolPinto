---
description: >-
  Esta tabla mantiene un control más estricto sobre los frames que posee un
  proceso.  Además los frames que no están cargados también se guardan aquí.
---

# Suplementary Page Table

La información de la Frame Table no es suficiente en la mayoría de los casos, ya sea en frames o páginas que no están cargadas o información extra de las páginas que estamos guardando. Esta tabla es propia a cada programa de usuario,  es decir, cada proceso va a tener su propia tabla de Suplementary Page Table. 

## Lo que incluye

{% tabs %}
{% tab title="Suplementary Page Table" %}
```c
struct hash sup_table;
```

Cada proceso/ thread tiene un hash table que tiene como llave la upage y de value una `spage_entry`
{% endtab %}

{% tab title="Suplementary Page Entry" %}
```c
struct spage_entry
{
    void* upage;

    bool loaded;
    bool writable;
    bool in_swap; 

    size_t swap_id; 

    Page_Type type; 

    struct file_page *file;

    struct hash_elem elem; 
};
```

Nuestra estructura para mantener control sobre la páginas que tiene un proceso. El puntero upage es la dirección virtual de usuario, funciona como llave en la hash table. También incluye un swap\_id que es el bloque en que  se encuentra la página en el swap file. El miembro más usado es loaded, que se  funciona para saber si esa página esta en swap, en un file o si está cargada en la memoria.
{% endtab %}

{% tab title="Page\_Type" %}
```c
typedef enum {
    PAGE,
    EXECUTABLE,
    MMFILE
} Page_Type;

```

Es una enum que identifica todo el tipo de páginas que podemos tener en la memoria. 

1. PAGE: Son páginas normales del stack.
2. EXECUTABLE: Pertenecen al código del proceso. Es todo el archivo de proceso. 
3. MMFILE: Pertenece a los archivos mapeados a memoria por medio de mmap\(\)
{% endtab %}

{% tab title="File\_page" %}
```c
struct file_page
{
    struct file *file; 
    off_t ofs; 
    uint8_t *upage; 
    uint32_t read_bytes; 
    uint32_t zero_bytes; 
    bool writable;
};
```

Esta estructura guarda información de 

1. Archivos ejecutables
2. Archivos mapeados a memoria

Incluye cuantos bytes hay que escribir y cuantos bytes hay que poner en cero.
{% endtab %}
{% endtabs %}

## Operaciones de la Suplementary Page Table

### Obtener una nueva SPage

Crea una nueva página de tipo `PAGE` y la inserta a Suplementary Page Table. 

### Obtener una nueva SPage para un archivo

Crea una nueva página suplementaria de tipo `EXECUTABLE`o `MMFILE`, ya que está guarda la información de este tipo de archivos. Inserta la Suplementary Page Entry a la Suplementary Page Table.

### Cargar a memoria una página de un archivo 

Este método solo se ejecuta con SPE de tipo `EXECUTABLE` y `MMFILE`. Si este tipo de página esta en SWAP entonces se carga como una página normal. 

Lo primero que hay que hacer es pedir un nuevo frame a la frame table. Luego hay que colocar el puntero del archivo en la la posición del offset que indica la SPE.  Luego leemos los bytes que necesitamos del archivo y ponemos en 0 la cantidad de bytes que nos indica la SPE. 

### Cargar a memoria una página

Este método carga a memoria páginas de tipo PAGE y EXECUTABLE que se pueden escribir o están en swap. 

Se pide un nuevo frame a la frame table. Si la página se encuentra en el swap se debe de sacar del swap y liberar el espacio en el swap file. 



