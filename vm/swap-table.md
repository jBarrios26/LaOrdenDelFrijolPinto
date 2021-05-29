---
description: >-
  Esta tabla nos sirve para poder asignar bloques de swap file a páginas que
  fueron victimas de desalojo.
---

# Swap Table

El Swap File se usa para simular que la máquina tiene más memoria de la que en realidad tiene el sistema . El sistema operativo tiene un Block dedicado para el archivo de Swap, el cual contiene varios sectores que tienen 512Mb de tamaño. 

```c
struct bitmap *swap_table
```

## Swap Allocate 

Cada página tiene un tamaño de 4Kb, entonces es necesario tener 8 sectores libres en el archivo de Swap. Buscamos que esos sectores sean continuos en el archivo. En el bitmap se van a negar esos bits para marcarlos en uso. 

## Swap Deallocate

En este caso hay que liberar los sectores del bitmap y escribir la información del swap a la memoria.  

