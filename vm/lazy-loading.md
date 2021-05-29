---
description: Uno de los requerimientos de la fase 3
---

# Lazy Loading

Lazy Loading es una procedimiento que se debe de hacer para cargar los ejecutables de un proceso a memoria conforme se van necesitando. 

El lazy loading se implementa creando una SPE en lugar de pedir un nuevo frame al momento de cargar todas las páginas del proceso. Cuando se hace un page fault se revisa que la dirección de fault sea una dirección del ejecutable y se realiza la carga de la SPE con `load_page_file` .

{% tabs %}
{% tab title="process.c" %}
```c
 bool load(){
 . . .
 /* Carga de segmentos: vm_flag marca si el lazy la carga. */
 if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable, vm_flag))
                goto done;
                
  . . .
}

bool load_segment ( . . . ){
. . .
          if (!get_file_page(file, ofs, page_read_bytes, page_zero_bytes, writable, EXECUTABLE,  upage))
                    return false;
. . .
}
```
{% endtab %}
{% endtabs %}

{% tabs %}
{% tab title="exception.c" %}
```c
struct spage_entry *page = lookup_page(cur, pg_round_down(fault_addr));
   if (page != NULL && !page->loaded)
   {
      switch (page->type)
      {
      case MMFILE:
      case EXECUTABLE:
         load_file_page(page);
         return;
      case PAGE:
         load_page(page);
         break;
      }
   }
```
{% endtab %}
{% endtabs %}



