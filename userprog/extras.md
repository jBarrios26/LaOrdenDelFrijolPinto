---
description: >-
  Estas son funciones extras que debe de cumplir la parte de USERPROG, pero que
  son extras a argument passing y a SYSCALLS.
---

# Extras

## Verificar Punteros

Esta es una función que ayuda a verificar que los punteros que el proceso de usuario envía a al _Syscall Handler_ sean válidos. Para que sean válidos deben de cumplir estas dos condiciones: 

1. El puntero no sea nulo o pertenezca a la memoria del _kernel._
2. El puntero pertenece a la página de memoria del proceso.

```c
bool 
verify_pointer(void *pointer)
{
  struct thread *cur = thread_current(); 
  bool valid = true;
  if (!is_user_vaddr(pointer) || pointer == NULL)
      return false;
  if (pagedir_get_page(cur->pagedir, pointer) == NULL)
      return false;
  return valid; 
}
```

## Denegar la escritura de ejecutables.

Cuando un proceso es cargado es importante que el mismo proceso o otros procesos puedan modificar el contenido del archivo ejecutable del proceso.  Este proceso se realiza en la función `load` cuando la carga sea exitosa.

### Algoritmo

1. Cargar el archivo. 
2. Si se cargo exitosamente
   1. Denegar  la escritura al archivo. 
   2. Crear un `file descriptor` e ingresarlo a la lista de `file descriptors` del thread
3. Cuando se termina el proceso , habilitar los permisos de escritura al archivo.

```c
 file_deny_write(file);
```

{% hint style="info" %}
Cuando se llama la función `file_close` , esta automáticamente habilita la escritura de un archivo abierto. 
{% endhint %}

## Verificar accesos a memoria

Esta verificación es similar a las verificaciones de punteros, solo que está verificación revisa los accesos a memoria mediante punteros que el proceso realiza. Estos accesos no son por medio de **syscalls** sino por medio de código. 

Los casos que hay que revisar son: 

1. Accede a un dirección nula
2. Accede a una dirección que no es del segmento de usuario.

La implementación de esta verificación se realiza en la función de `page_fault`del archivo `exception.c`, si sucede uno de esos dos casos, entonces se llama a la función `exit(-1).`

