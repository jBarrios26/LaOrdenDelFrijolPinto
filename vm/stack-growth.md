---
description: >-
  Los procesos actualmente tienen un stack fijo, ahora es necesario que su stack
  pueda crecer de forma automática.
---

# Stack Growth

Para que el stack pueda crecer es necesario que cumpla las siguientes condiciones: 

1. Exista un SPE en la Suplementary Page Table del proceso. 
2. Que la dirección este al menos a 32 bytes del stack pointer
3. El tamaño del stack sea menor que  8MB

Para que crezca el stack es necesario pedir un nuevo frame e instalarlo. Se va a llamar cada vez que ocurra un page fault y cumpla las condiciones. 

{% tabs %}
{% tab title="stack\_growth" %}
```c
bool stack_growth(void *fault_address)
{
   void *upage = pg_round_down(fault_address); 
   uint32_t* frame = create_frame(); 
   if (frame != NULL){
      bool success = install_frame(frame, upage, true); 
      if (!success){
         destroy_frame(frame);
         return false;
      }
      get_page(upage, true); 
      
      return true;
   }else 
      return false; 
}
```
{% endtab %}

{% tab title="page\_fault" %}
```c
if (page == NULL && (esp - 32)  <= fault_addr && 
      (void*)(PHYS_BASE - fault_addr) <= (void*)0x80408000){
      stack_growth(fault_addr);
```
{% endtab %}
{% endtabs %}



