---
description: La primera asignación del proyecto de Threads.
---

# Alarm Clock

## ¿ En qué consiste?

Esta asignación consiste en que los threads puedan dormir una cantidad de ticks sin utilizar  **busy waiting.**   Para lograr esto hay que utilizar la cola de threads con estado **BLOCK** y colocar en esa cola el thread, y cuando pase tiempo que el indico para estar dormido cambiar su estado de **BLOCK** a **READY**. 

### ALGORITMOS:

{% tabs %}
{% tab title="SLEEP\_THREAD" %}
```text
SLEEP_THREAD:
    INDICAR EL TIEMPO POR DORMIR
    AGREGAR EL THREAD A LISTA DE THREADS DURMIENDO
    CAMBIAR DE READY A BLOCK
```
{% endtab %}

{% tab title="WAKE\_THREAD" %}
```
WAKE_THREAD(ticks):
    thread aux = LISTA_THREAD_DURMIENDO[0]
    MIENTRAS aux = LISTA_THREAD_DURMIENDO.LAST:
        SI aux.sleep_time == ticks ENTONCES 
            REMOVER aux DE LA LISTA
            DESBLOQUEAR EL THREAD
```
{% endtab %}
{% endtabs %}

{% hint style="info" %}
wake\_thread\(ticks\) es llamado en cada interrupción del timer. Esto asegura que el thread durmió al menos el tiempo que el pidió.
{% endhint %}



