#include "tymalloc-internal.h"

/* Manages ty_heap_t creations*/
// Thread local storage and size class bin array indexing -> (page direct)

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
    void *p = mmap(NULL, ty_os_page_size(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("heap init: mmap");
        return NULL;  
    }

    ty_heap_t* heap = (ty_heap_t*) p;
    
    heap->thread_id = get_current_thread_id(); 
    return heap; 
}

/* Finds a free page within segment or make a new segment */
ty_page_t* ty_heap_get_page(ty_heap_t* heap, size_t block_size) {
    ty_page_kind_t required_kind = ty_page_kind_for_size(block_size);
    
    for (ty_segment_t* seg = heap->segments; seg; seg = seg->next) { // search segments to find a unused page 
        if (seg->page_kind == required_kind && seg->page_used < seg->page_amounts) {
            ty_page_t* page = ty_segment_claim_page(seg);
            if (page == NULL) { // Segment is full, No available page
                continue;       // Search next segment 
            }
            ty_page_init(page, block_size); // Found a compatible segment with space
            return page; 
        }
    }

    // Make a new segment, claim a page, init the page, carve the first batches of blocks; 
    ty_segment_t* seg = ty_segment_create(heap, required_kind, block_size);
    ty_page_t* page = ty_segment_claim_page(seg);
    ty_page_init(page, block_size); 
    return page;
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
        cached_thread_id = atomic_fetch_add(&global_thread_counter, 1);
    }
    return cached_thread_id; 
}






















