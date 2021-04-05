---
description: >-
  La forma en que un programa de usuario se comunica con el sistema operativo
  para acceder a memoria o al file system es con las llamadas al sistema 
  (System Calls o syscalls).
---

# System Calls

En esta asignación se deben de crear 13 llamadas al sistema para que los programas de usuario se puedan comunicar con el sistema operativo.  Estas llamadas al sistema operativo tiene que brindar protección en caso de que un programa de usuario quiera acceder a memoria que no le pertenece y también utilizar métodos de sincronización adecuados para acceder al file system. 

## System Call Handler

Cuando un programa de usuario realiza una **syscall** el programa levanta una interrupción para que el sistema operativo la maneje y realice la llamada que el hizo. El programa de usuario antes de hacer el **syscall** escribe al stack los argumentos del **syscall** que va a realizar y en el tope del stack escribe el código del **syscall**

| **SYS\_CODE** | **&lt;- esp** |
| :---: | :--- |
| ARG\_0 | &lt;- esp + 1 |
| ARG\_1 | &lt;- esp + 2 |
| ARG\_2 | &lt;- esp + 3 |

 **Cuando e**l sistema detecta la interrupción, tiene un`syscall_handler` que maneja la interrupción del **syscall** para ejecutar el **syscall** que viene indicado en el **`SYS CODE`** de la interrupción. 

En esta fase hay 13 **System Calls**:

| SYSTEM CALL |
| :---: |
|  void[ **halt**](syscalls.md#halt) \(void\) |
| void [**exit**](syscalls.md#exit) \(int status\) |
|  pid\_t [**exec**](syscalls.md#exec) \(const char \*cmd\_line\) |
| int [**wait**](syscalls.md#wait) \(pid\_t pid\) |
| bool[ **create** ](syscalls.md#create)\(const char \*file, unsigned initial\_size\) |
| bool[ **remove** ](syscalls.md#remove)\(const char \*file\) |
| int [**open**](syscalls.md#open) \(const char \*file\) |
|  int [**filesize**](syscalls.md#filesize) \(int fd\) |
| int[ **read**](syscalls.md#read) \(int fd, void \*buffer, unsigned size\) |
| int [**write**](syscalls.md#write) \(int fd, const void \*buffer, unsigned size\) |
|  void[ **seek**](syscalls.md#seek) \(int fd, unsigned position\) |
| unsigned [**tell**](syscalls.md#tell) \(int fd\) |
| void [**close**](syscalls.md#close) \(int fd\) |

El system\_handler también se encarga de revisar que los punteros que provee el programa de usuario sean válidos \(ver [Extras](extras.md)\).

## Halt

Es el **syscall** más sencillo. Solo debe de realizar una llamada a la función de `shutdown_power_off()` para apagar a la máquina.

## Exit

Se encarga de terminar el proceso actual y le retorna el estado al kernel. Si un proceso está esperando a este proceso, exit se encarga de avisarle el estado de salida al proceso padre.

En esta parte también es necesario liberar todos los recursos que un proceso posee, por ejemplo, liberar todos los locks y/o cerrar todos los archivos que tenga abiertos. 

También se encarga de imprimir el mensaje de salida de un proceso con el status de salida.

### Algoritmo

1. Revisar si el proceso tiene padre.
   1. Si tiene padre, entonces conseguir su `children_process` del hash children del padre. 
   2. Obtener un lock para modificar las variables de condición del proceso padre.
   3. Modificar la variable finish y status del `children_process`
   4. Realizar un `cond_signal` al proceso padre, esto lo va a despertar en caso de que esté esperando \(ver [Wait](syscalls.md#wait)\).
2. Comunicarle a los procesos hijos que "ya no tienen padre". Se utiliza el hash de children que tiene cada proceso.
3. Liberar los locks que tiene el proceso en su lista de locks.
4. Si el proceso tiene un ejecutable, entonces liberar el archivo ejecutable y habilitar la escritura. \(ver [Extras](extras.md)\)
5. Liberar todos los files que tiene el proceso en su lista de files.

{% hint style="info" %}
Para avisarle a los procesos hijos que ya no tienen padre se utiliza la función `hash_apply(struct hash *hash, void *hash_apply_func)`
{% endhint %}

## Exec

Este **syscall** es llamado cuando un proceso quiere iniciar un nuevo proceso hijo. Hay que utilizar métodos de sincronización para asegurar que exec siempre retorne el **pid\_t** del nuevo proceso o -1 si ocurrió un error al cargarlo. 

Este **syscall** involucra que se modifiquen dos funciones que afectan tanto al proceso padre como al proceso hijo. En el proceso padre se  debe de modificar `process_execute` y la función `exec`, mientras que en el proceso hijo se debe de modificar la función  `start_process`.

### Algoritmo 

1. Llamar a `process_execute` para crear el nuevo proceso. 
2. Crear un nuevo `children_process` que lleva el control del proceso hijo, como el estado o si ya termino de ejecutarse. 
3. Usando métodos de sincronización esperar que se termine de cargar el proceso hijo. 
   1. Si no se cargo exitosamente, entonces liberar el `children_process` 
4. Retornar el **pid\_t** del proceso hijo.

### Cambios a process\_execute

Si el thread actual \(parent\) no ha iniciado su  `children` HashMap entonces iniciar el nuevo HashMap y crear el nuevo  `children_process`

### Cambios a start\_process

El proceso de carga del proceso se realiza de la misma manera. Después de terminar la carga de revisa si el proceso tiene un padre entonces hay que modificar la variable `child_load` del padre a `true` y si se carga de manera exitosa entonces se modifica la variable `child_status` del padre a `true`. Para avisarle al padre que ya se termino de cargar el programa se utiliza un semáforo.

### Sincronización

Cada proceso tiene un semáforo para poder sincronizar la tarea de crear un nuevo proceso. Para sincronizar la creación del nuevo proceso, el thread padre baja el semáforo en la función de `exec` y el proceso hijo sube el semáforo en la función de `start_process`

## Wait

Este **syscall** es invocado cuando un proceso padre quiere esperar a que uno de sus procesos hijos termine su ejecución. Retorna el estado del hijo al momento de salir .

### Casos

* Padre espera al hijo que no ha terminado. Retorna status del hijo al salir.
* Padre espera a un hijo que ya terminó. Retorna status del hijo cuando salió. 
* Padre espera a un hijo que no existe. Retorna -1.
* Padre espera a un hijo más de una vez. Retorna -1. 

La implementación de wait se realiza en su mayoría en la función `process_wait`. Y se complementa en el **syscall** de exit, que se usa cuando el hijo termina su ejecución y le avisa al padre. 

### Algoritmo

1. Revisar que el padre tenga a un hijo con el mismo **pid\_t**
2. Revisar que el padre no haya esperado al hijo más de una vez. 
3. Revisar si el hijo ya terminó su ejecución.
   1. Retornar el status de salida del hijo. 
4. Espera al hijo utilizando métodos de sincronización. 
5. Retornar el status del hijo.

### Sincronización 

Para lograr que el thread padre espere a un proceso hijo se utiliza un monitor. En `process_wait` se obtiene un lock y se espera la variable de condición hasta que el hijo termine la ejecución. 

En el **syscall** de `exit`, el hijo realiza un signal al padre para  que termine de esperar. 

```c
  lock_acquire(&cur->wait_lock);
    while (!child_control->finish)
    {
      cond_wait(&cur->wait_cond, &cur->wait_lock);
    }
  lock_release(&cur->wait_lock);
```

## Create

Este es un **syscall** que tiene interacción con el file system. Crea un nuevo archivo con un nombre `file` y con un tamaño inicial de `initial_size`. Hay que utilizar un lock para evitar que otro thread cree un archivo con el mismo nombre y cause conflicto. Retorna `true`si se creó exitosamente y `false`si ocurrió un error.

{% hint style="info" %}
Utiliza la función `filesys_create(char *file, unsigned initial size)` del archivo `filesys.c` 
{% endhint %}

## Remove

Este **syscall** se encarga de remover un archivo del file system. Remueve el archivo que tenga el mismo nombre que `file`. Se debe de utilizar un lock para evitar que dos threads intenten remover el archivo al mismo tiempo. 

Si un archivo es removido todos los threads que tengan abierto el archivo, incluido el que lo borró, lo pueden seguir utilizando para escribir o leer. Es decir, remover un archivo no lo cierra.

## Open

El **syscall** de open permite a un programa de usuario abrir un archivo que tiene nombre `file`. Para identificar los archivos que tiene un proceso de usuario abierto se utilizan `file descriptors`. Los `file descriptors` pueden ser números enteros o estructuras más complejas, en nuestro caso, utilizamos una `struct` especifica \(ver [Structs](usrprog-structs.md)\) para mantener un mejor control sobre los archivos que un proceso tiene abiertos. 

Para mantener el control de los archivos adentro se decidió tener una lista global de todos los archivos abiertos y cada proceso tiene una lista propia con todos sus archivos abiertos. El `fd` tiene un identificador entero que depende del thread  \(cada thread tiene un correlativo de archivos abiertos\). 

 Hay que tomar en cuenta también que los `fd` 0  y 1 están reservados para la consola. 

| FD | NAME |
| :--- | :--- |
| 0 | STDIN\_FILENO |
| 1 | STDOUT\_FILENO |

{% hint style="info" %}
Para abrir un archivo se utiliza la función `filesys_open(char *name)` del archivo `filesys.c`
{% endhint %}

{% hint style="warning" %}
`Si un archivo lo abrió otro proceso, entonces se tiene que reabrir el archivo con` filesys\_reopen\(struct file \*file\)
{% endhint %}

## Filesize

Retorna el tamaño del archivo que tenga el mismo `fd`.

## Read

Este **syscall** permite a un proceso de usuario leer un archivo que tenga el mismo `fd` que el envío al **syscall**. Va a leer la cantidad de bytes que este en `size`y coloca lo que se leyó en el `buffer`. Hay que tomar en cuenta que si `fd` es 0 se debe de leer del teclado, y si no hay ningún archivo que tenga el mismo `fd`o se este leyendo de `fd` igual a 1, hay que retornar -1. 

{% hint style="info" %}
Para leer de la consola se debe de usar la función `input_getc()` 
{% endhint %}

{% hint style="info" %}
Para leer de un archivo se utiliza la función `file_read(struct file *file, void* buffer, unsigned size)`
{% endhint %}

## Write

Este **syscall** permite a un proceso escribir un `buffer`a un archivo que tenga el mismo `fd` que le envió a la función.  Esta función retorna la cantidad de bytes que se escribieron al archivo, puede ser menos bytes que el  `size` que se le envío al **syscall**. 

En caso de que se escriba al  `fd` igual 1 ,  se debe de escribir el `buffer` a la consola. 

Si se intenta escribir a un fd que no lo haya abierto el proceso antes o este escribiendo a  `STDIN_FILENO` se retorna 0, porque no se escribieron bytes. 

{% hint style="info" %}
Para escribir a un archivo se utiliza la función `file_write(struct file *file)`
{% endhint %}

{% hint style="info" %}
Para escribir a la consola se utiliza la función  `putbuf(char *str, unsigned size)`
{% endhint %}

## Seek 

Este **syscall** cambia el siguiente byte  a se leído o escrito en un archivo abierto `fd.`

{% hint style="info" %}
Se utiliza la función  `file_seek(struct file *file, unsigned position)`
{% endhint %}

## Tell

Retorna el siguiente byte a ser leído o escrito en un archivo abierto `fd`.

{% hint style="info" %}
Se utiliza la función `file_tell(struct file *file)`
{% endhint %}

## Close

Este **syscall** cierra un archivo abierto `fd` que este en la lista de archivos del proceso. Esta función es llamada por cada archivo abierto cuando un archivo llama al **syscall** de `exit` . 





