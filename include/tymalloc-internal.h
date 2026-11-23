#ifndef TYMALLOC_INTERNAL_H
#define TYMALLOC_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h> 
#include <unistd.h>



#define TY_SEGMENT_SIZE (1 << 22) // 4MiB
#define TY_NO_PAGE SIZE_MAX
#define TY_MAX_ALIGN _Alignof(max_align_t) 

#define MIN(a, b) ((a) < (b) ? (a) : (b))

//Forward decleartions
typedef struct ty_block_s ty_block_t; 
typedef struct ty_page_s ty_page_t;
typedef struct ty_segment_s ty_segment_t;
typedef struct ty_heap_s ty_heap_t;

//.       Question: Why use Enum here?
//                  Instead of Macro 1ULL << 22 per size 
typedef enum ty_page_kind_e {
    TY_PAGE_SMALL,
    TY_PAGE_LARGE,
    TY_PAGE_HUGE
} ty_page_kind_t;

struct ty_block_s {
    ty_block_t* next; 
    ty_block_t* prev; 
};

struct ty_page_s {
    // List linkage between pages of same size class or full list
    ty_page_t* next; 
    ty_page_t* prev; 

    // Sharded free lists
    ty_block_t* free;       // Main allocation list
    ty_block_t* local_free; // Local deallocation list
    _Atomic(ty_block_t*) thread_free; // Atomic remote list 

    // Metadata 
    size_t used;         // Count of currently active allocations to the user  
    size_t reserved;     // How many blocks have been carved from payload so far
    size_t capacity;     // The maximum number of blocks that can fit inside this page area
    size_t thread_freed; // Tracks counter for remote free
    uint8_t* page_start;  // Where page's payload starts
    uint8_t* carve_pointer; // Carve pointer moving 

    size_t block_size;   // Uniform byte size of every object in this page (Size class)
    uint8_t size_bin;    // Index into heap->pages[size_bin]
     
    bool is_full;        // True if page is in TY_BIN_FULL
}; 

struct ty_segment_s {
    uint32_t thread_id;
    size_t page_shift;   // Bit shift exponent 16 for small, 22 for large+ 
    ty_page_kind_t page_kind; 
    size_t segment_size; // for mummap custom segments 

    // Page bookkeeping
    uint8_t page_amounts;      // Number of pages in this segment 
    uint8_t page_used;         // Number of page used  /* Refactor: used+capacity can be derived from page_in_use */
    uint64_t page_in_use; // Bit vector tracking pages in use within segment; 0 is Free

    // List linkage between active segments
    ty_segment_t* next;

    ty_page_t pages[64];
}; 

struct ty_heap_s {
    ty_page_t* pages_direct[128];  // O(1) direct lookup 
    ty_page_t* pages[74];          // Size class bins + Full slot
    _Atomic(void*) thread_delayed_free;
    uint32_t thread_id;
    ty_segment_t* segments; // Head of Linked list of active segments of current thread

    /* Why is there no segment inside heap? */
};


// Function prototypes

// Heap.c 
ty_heap_t* ty_heap_get_default(void);
size_t ty_os_page_size(void);
uint32_t get_current_thread_id(void);

// Segment.c 
ty_segment_t* ty_segment_create(ty_heap_t* heap, ty_page_kind_t page_kind, size_t required_size);
ty_page_t* ty_segment_claim_page(ty_segment_t* seg);


// Page.c 
ty_page_kind_t ty_page_kind_for_size(size_t block_size);
void ty_page_init(ty_page_t* page, size_t block_size);     



// Helpers

/* Aligns - rounds up to a factor of alignment */
static inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// Finds _Segment_ based on page 
static inline ty_segment_t* segment_lookup(ty_page_t* page) {
    uintptr_t mask = ~(TY_SEGMENT_SIZE - 1);
    ty_segment_t* segment = (ty_segment_t*)(((uintptr_t)page) & mask); 
    return segment;
}

// Find page slot index
static inline int page_slot_index_lookup(ty_page_t* page) {
    ty_segment_t* segment = segment_lookup(page);
    return ((uintptr_t)page - (uintptr_t)segment) >> segment->page_shift;

}

#endif