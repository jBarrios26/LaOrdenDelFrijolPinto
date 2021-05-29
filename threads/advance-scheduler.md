---
description: >-
  El calendarizador MLFQS ayuda a reducir el tiempo de respuesta promedio de
  ejecución en el sistema. Este calendarizador también se basa en prioridades,
  pero sin hacer donaciones.
---

# Advanced Scheduler

El MLFQS escoge cada cierto tiempo a un thread de la cola que tenga la prioridad más alta, siempre y cuando esta cola no tenga más de un thread ya que de ser así entonces se calendarizan con un orden Round Robin de nuevo.

Algo importante de este calendarizador es que requiere que los valores de los threads se actualicen cada cierto tiempo en la sincronización. En las secciones siguientes se explican más a detalle cómo manejamos esto.

{% hint style="info" %}
Este tipo de calendarizador no va a ser usado en ninguna otra parte del proyecto.
{% endhint %}

## Niceness

Para evitar la donación de prioridades, el MLFQS calcula la prioridad de los thread con una nueva formula, pero también agrega un nuevo valor a cada thread para ayudar a saber quién será el siguiente en ser ejecutado, este es el valor de NICE. Este valor nos dice que tan "amable" el thread actual debería ser con los demás, para saber si debe ceder el tiempo de CPU o si es el siguiente en ser ejecutado, nos basamos en las siguientes reglas. Los valores de nice pueden ir de -20 a 20.

1. Si nice = 0, no va a afectar la prioridad en ninguna forma.
2. si 0 &gt; nice &lt; 20, entonces va a decrementar la prioridad del thread y este cederá su tiempo de CPU.
3. si -20 &gt; nice &lt; 0, Entonces su prioridad va a aumentar y entonces pedirá tiempo de CPU de otros threads.

{% hint style="warning" %}
Todos los threads que sean inicializados empezarán con el default nice, es decir nice = 0. De lo contrario, heredaran el valor que tenga the thread padre.
{% endhint %}

### Funciones a implementar

{% tabs %}
{% tab title="Get Nice " %}
La función `thread_get_nice ()` devolverá el valor `nice` del `current_thread.`
{% endtab %}

{% tab title="Set Nice" %}
`thread_set_nice (int nice)` nos ayudara a establecer el valor de nice para cada thread y volverá a calcular la prioridad del thread basándose en ese valor. Si el running thread no tiene la prioridad más alta, entonces cederá el paso a otro thread.
{% endtab %}
{% endtabs %}

## Prioridades

La prioridad de cada thread será calculada al inicializarlo, pero también necesitaremos recalcularla cada 4 ticks para todos los threads.

Para hacer este calculo utilizamos esta formula:

```text
priority = PRI_MAX - (recent_cpu / 4) - (nice * 2)
```

`recent_cpu` es un valor que explicaremos a continuación, pero estima el tiempo de CPU que el thread ha utilizado recientemente. Este resultado debería ser redondeado al entero más cercano.

Con esta forma de calcular la prioridad de cada thread podemos asignar las prioridades más bajas a los threads que hayan recibido tiempo de CPU recientemente. Esto ayuda mucho para prevenir starvation.

Nosotros implementamos `recalculate_priority(thread)`

## Fixed Point Arithmetic 

Antes de explicar cómo hacemos los nuevos calculos de prioridades y otros más importantes, hay que tomar en cuenta que Pintos no soporta punto flotante, y ya que en las formulas necesitamos operar con valores integer como `nice`, `priority` y `ready_threads` y valores con punto flotante, nosotros implementamos unas macros para poder controlar esto las cuales se ecuentran en `threads/fixed-point.h`

La idea es tomar a los bits más significativos de todos los threads como una fracción con una representación fixed-point, que toma a los 14 bits menos significativos de los integer como la parte fraccionaria y los 17 bits estarán antes del punto decimal, más 1 bit para el signo.

Luego nos basamos en el orden de las operaciones según estaba en la [documentación](https://web.stanford.edu/class/cs140/projects/pintos/pintos_7.html#SEC137).

## Recent CPU 

Cada thread debe tener un valor que nos diga cuánto tiempo ha usado del CPU recientemente para saber si es justo que sea el siguiente en ejecutarse o no. Para esto utilizamos la siguiente fórmula

`recent_cpu = (2`_`load_avg)/(2`_`load_avg + 1) * recent_cpu + nice`

De nuevo, `load_avg` lo explicaremos luego.

Al inicializar todos los threads deberían tener un valor de recent\_cpu = 0 ya que aún no han tenido tiempo de CPU, de lo contrario tendrán el valor que tenga su thread padre.

Algo importante a mencionar es que al igual que las prioridades, el valor de `recent_cpu` debe incrementarse por 1 únicamente para el thread que esté en running. También se debería recalcular este valor para cada thread cada segundo sin importar el estado en el que estén los threads. Esto es para asegurarnos que estamos asignando bien las prioridades.

El valor de `recent_cpu` puede ser negativo si el valor de `nice` del thread es negativo también.

## Load Average

El load average hace una estimación del numero de los threads que estaban en `ready_list` en el último minuto. Este valor no es propio de cada thread, sino que de todo el sistema en sí por lo que se inicializa en 0 y cada segundo es actualizado con esta fórmula.

`load_avg = (59/60)`_`load_avg + (1/60)`_`ready_threads`

`ready_threads` es el número de threads que estaban en running o ready en el momento que de la interrupción.

## Thread MLFQS

Ahora que ya tenemos todas las implementaciones para realizar los calculos en los threads, es hora de asegurarnos de "activar" el calendarizador.

Utilizaremos bool `thread_mlfqs`, que por default es false para que PintOs utilice una calendarización Round Robin, pero al ser true utilizará el multi-level feedback queue scheduler.

## Interrupciones

En `timer_interrup()` vamos a utilizar algunos de los calculos que ya hicimos. Aquí vamos a controlar que si un thread está en running, entonces el `recent_cpu` se incrementará en 1.

También vamos a asegurarnos de actualizar el `recent_cpu` para todos los threads y también vamos a calcular el load average, esto sucederá cuando `ticks % TIMER_FREQ == 0`. También desde aquí controlamos que las prioridades de todos los threads se actualicen cada 4 ticks.

## Crear un nuevo Thread 

Ahora para terminar nuestra implementación, y la parte más importante de todas es la siguiente.

### thread\_create\(\)

En thread\_create debemos "activar" de nuevo el calendarizador MLFQS. Aquí calcularemos las prioridades y el valor de `recent_cpu` para todos los threads.

```c
if (thread_mlfqs){
    calculate_recent_cpu(t, NULL);
    calculate_recent_cpu (thread_current (), NULL);
    recalculate_priority(t, NULL);
    recalculate_priority(thread_current(),NULL);
  }
```

### init\_thread\(\)

Aquí vamos a definir que si el thread se está inicializando su valor de `nice` y de `recent_cpu` será 0 respectivamente, y si no, entonces podemos obtenerlos con `thread_get_nice()` y `thread_get_recent_cpu()`

```c
if (thread_mlfqs){
    if (t== initial_thread){
      t->nice = 0;
      t->recent_cpu = 0;
    }else{
      t->nice = thread_get_nice ();
      t->recent_cpu = thread_get_recent_cpu ();
    }
}
```

