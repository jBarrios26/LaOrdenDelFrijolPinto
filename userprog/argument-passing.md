---
description: >-
  La primera asignación de los programas de usuario es lograr pasar los
  argumentos de consola a los procesos al momento de mandarlos a ejecutar.
---

# Argument Passing

En los programas que maneja PintOS, los argumentos que se le pasan al programa son **argc** y **argv.** Estos argumentos se deben de guardar en inicio del stack del proceso, en el caso de PintOS el stack empieza en `PHYS_BASE`  . El argument passing debe de cumplir el convenio de llamadas a funciones de Intel 80x86: 

* Caller hace push de los argumentos con un orden inverso \(derecha a izquierda\).
* Caller hace push a la dirección de retorno al tope del stack. 
* El Callee ejecuta su función.
* El Callee guarda su resultado de retorno, si tiene, en **EAX** \(a0\).
* El Callee hace pop a la dirección de retorno y regresa a esa instrucción.
* El Caller saca los argumentos del stack.

![Stack de un proceso nuevo.](../.gitbook/assets/image%20%283%29.png)

{% hint style="info" %}
El stack pointer en PintOS está representado por el doble puntero `**esp`.
{% endhint %}

{% hint style="info" %}
`PHYS_BASE` empieza en `0xc0000000`
{% endhint %}

## ¿Cómo se cargan los procesos en PintOS?

Para entender como pasar los argumentos a un proceso, es importante saber como los procesos son cargados por el Sistema Operativo.

1. El thread padre llama a la función `process_execute(char *file_name)` . En este función se crea el nuevo thread con el nombre de `file_name`  y se configura para que ejecute la función de `start_process(char* file_name)` y le envía al thread como parámetro  a `file_name` 
2. El nuevo thread empieza a ejecutar `start_process(char* file_name)` donde se empieza a cargar el proceso con la función `load(char *file_name)` 
3. En la función `load(char *file_name)` se abre el archivo y se carga a memoria, si esta acción es exitosa, entonces se empieza a cargar los argumentos al stack con la función `setup_stack (void **esp, char* file_name)`
4.  En la función de `setup_stack (void **esp, char* file_name),` asume que los argumentos estan en el file\_name y luego hace un push de cada argumento al stack y los punteros que apuntan a los argumentos. 
5. Si todo el proceso de load se realizó de manera exitosa, se empieza a correr el proceso,  de lo contrario, se llama a la función `thread_exit()` para destruir el proceso.

{% hint style="warning" %}
Este es el comportamiento deseado, pero no es el comportamiento original que tiene la carga de procesos al iniciar la asignación. 
{% endhint %}

## Los primeros pasos antes de pasar los argumentos

La primera tarea para pasar los argumentos es ir a la función `process_execute(char* file_name)`. En esta función se crea el thread que maneja al nuevo proceso con el nombre del proceso, pero el filename no solo incluye el nombre sino también incluye la lista de argumentos que se le pasaron en consola o en el syscall de exec.

{% hint style="info" %}
Por ejemplo: Una llamada a un nuevo proceso puede ser el siguiente: `new_process arg1 arg2 arg3`
{% endhint %}

Entonces es necesario separar el nombre del archivo de sus argumentos. Para lograr esto se utiliza la función `strtok_r(char* str, char* delim, char** save_ptr).` Y al conseguir el nombre con este tokenizer ya se crea el thread con el token del nombre y se le envia una copia de los parámetros al nuevo thread.

{% tabs %}
{% tab title="process\_execute\(char\* filen" %}
```c
  strlcpy (fn_copy, file_name, PGSIZE);
  /* Get filename */
  name = strtok_r (fn_copy, " ", &save_ptr);
  strlcpy(args, save_ptr, strlen(file_name) - strlen(name));
```

`fn_copy` contiene una copia del `file_name`
{% endtab %}
{% endtabs %}

En este momento, el control de la creación del proceso queda en manos del thread que se acaba de crear. 

El nuevo thread empieza la creación del nuevo proceso en la función `start_process(char* filename)`. En la creación del nuevo thread `file_name` ya no contiene el nombre del archivo sino contiene la lista de parámetros del proceso. Para cargar el proceso se llama a la función `load (const char* file_name, void (*eip) (void), void **esp)` _``_ 

En la función de load,  __se le pasaron la lista de argumentos en el parámetro de`file_name.` En está función se abre el archivo y se carga el código a memoria, y por último se crea el stack.

{% hint style="info" %}
Para hacer referencia al nombre del proceso se utiliza el nombre del thread actual.
{% endhint %}

## Configurar el stack

En la configuración del stack se deben de pasar los argumentos al proceso y es el paso final antes de empezar a ejecutar el proceso. Está configuración se realiza en la función `setup_stack (void **esp, char *args, char *name)`   .

{% hint style="info" %}
* `**esp` es el stack pointer
* `*args` es la lista de argumentos 
* `*name` es el nombre del proceso
{% endhint %}

### Algoritmo para configurar el stack de un proceso.

1. Indicar que el stack pointer \(`**esp`\) apunta a `PHYS_BASE` \(la dirección base de la memoria\).
2. Escribir los argumentos del proceso con un orden inverso \(derecha a izquierda\).
3. Escribir el nombre del proceso.
4. Alinear la memoria, respecto a 4 bytes, con 0. 
5. Escribir de la dirección de `argv`
6. Escribir la dirección de cada argumento del proceso.
7. Escribir la dirección de `argv[0]`  
8. Escribir `argc`  \(la cantidad de argumentos más el nombre\)
9. Escribir la dirección de retorno \(para estos procesos es 0\).

{% hint style="info" %}
Para mover el stack pointer se hace lo siguiente: `*esp -= # bytes` 
{% endhint %}

### Paso \#1

```c
*esp = PHYS_BASE
```

### Paso \#2

{% tabs %}
{% tab title="Paso \#2" %}
```c
args_address[NUM_ARGS];
reverse_arg = reverse_args(args);
for (arg in args)
    *esp -= strlen(arg) + 1; 
    memcpy(*esp, arg, strlen(arg) + 1);
    args_adress[# of arg] = *esp
```

Se voltean los argumentos y se crea un arreglo para guardar las direcciones de cada arreglo. Por cada argumento en la lista de argumentos inversos se debe de bajar el stack pointer. Se baja el stack por el tamaño del arg más 1 porque hay que tomar en cuenta el NULL Terminator de un string. 
{% endtab %}
{% endtabs %}

### Paso  \#3

```c
*esp-= strlen(name)+1;
memcpy(*esp, name, strlen(name)+1);
addresses[argc++] = *esp;

```

### Paso \#4

{% tabs %}
{% tab title="Paso \#4" %}
```c
int mem_align =  (-1 * (int)*esp) % 4;
*esp -= mem_align;
memset(*esp, 0, mem_align);
```

Se debe de alinear la memoria porque al ingresar los argumentos o el nombre, el stack pudo quedar desalineado a la memoria, en lugar de apuntar al inicio de un word queda apuntando a otra parte de un word.

La operación para alinear la memoria es el módulo 4 del stack pointer para saber a que parte está apuntando. Se utiliza módulo 4 porque la memoria esta compuesta por words de 4 bytes.

La memoria se alinea con 0
{% endtab %}
{% endtabs %}

### Paso \#5

```c
*esp -= 4;
memset (*esp, 0, 4);
```

### Paso \#6

```c
int i = 0;
while (i < argc)
{
  *esp -= sizeof(char *);
  memcpy(*esp,  &arg_address[i++], sizeof(char*));
}
```

{% hint style="info" %}
El puntero se baja por el tamaño de un puntero char.
{% endhint %}

### Paso \#7

```c
void *argv0 = *esp;
*esp -= sizeof(char**);
memcpy(*esp, &argv0, sizeof(char**));
```

### Paso \#8

```c
*esp-=sizeof(argc);
memcpy(*esp, &argc, 4);
```

{% hint style="info" %}
La cantidad de argumentos argc se fue contando mientras se escribían los argumentos al stack.
{% endhint %}

### Paso \#9

```c
*esp -= sizeof(void*);
memset(*esp, 0, sizeof(void*));
```

{% hint style="info" %}
La dirección de retorno es 0 y es un puntero void.
{% endhint %}

