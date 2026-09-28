

#include <sys/mman.h> 
#include <unistd.h>
#include <stdatomic.h>
#include "tymalloc-internal.h"

static ty_heap_t* ty_heap_init(void);
static _Thread_local ty_heap_t* tlb = NULL; 


ty_heap_t* ty_heap_get_default(void) {
    if (tlb == NULL) {
        tlb = ty_heap_init();
    }
    return tlb; 
} 

/* Init the heap with jump tables */
static ty_heap_t* ty_heap_init(void) {
    
    void *p = mmap(NULL, ty_os_page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("heap init: mmap");
        return NULL;  
    }

    ty_heap_t* heap = (ty_heap_t*) p;
    
    heap->thread_id = get_current_thread_id(); 
    
    return heap; 
}

/* Cache OS page size */
size_t ty_os_page_size(void) {
    static size_t os_page_size = 0;
    if (os_page_size == 0) {
        os_page_size = (size_t)sysconf(_SC_PAGESIZE);
    }
    return os_page_size; 
}

static _Atomic uint32_t global_thread_counter = 1; 
static _Thread_local uint32_t cached_thread_id = 0; 

uint32_t get_current_thread_id(void) {
    if (cached_thread_id == 0) {
        return atomic_fetch_and_add(&global_thread_counter, 1);
    }
    return cached_thread_id; 
    
}






















