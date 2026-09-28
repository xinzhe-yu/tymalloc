#ifndef TYMALLOC_INTERNAL_H
#define TYMALLOC_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

//Forward decleartions
typedef struct ty_page_s ty_page_t;
typedef struct ty_segment_s ty_segment_t;

// The heap struct
typedef struct ty_heap_s {
    ty_page_t* pages_direct[128];  // O(1) direct lookup 
    ty_page_t* pages[74];          // Size class bins + Full slot
    _Atomic(void*) thread_delayed_free;
    uint32_t thread_id;

} ty_heap_t;


ty_heap_t* ty_heap_get_default(void);

// Helper
size_t ty_os_page_size(void);
uint32_t get_current_thread_id(void);


#endif