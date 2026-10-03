#ifndef TYMALLOC_INTERNAL_H
#define TYMALLOC_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#define TY_PAGE_SIZE (1 << 16) // 64KiB
#define TY_SEGMENT_SIZE (1 << 22) // 4MiB

//Forward decleartions
typedef struct ty_block_s ty_block_t; 
typedef struct ty_page_s ty_page_t;
typedef struct ty_segment_s ty_segment_t;
typedef struct ty_heap_s ty_heap_t;

typedef enum ty_page_kind_e {
    TY_PAGE_SMALL,
    TY_PAGE_LARGE,
    TY_PAGE_HUGE
} ty_page_kind_t;

struct ty_block_s {
    ty_block_t* next; 
};

struct ty_page_s {
    // List linkage between pages of same size class or full list
    ty_page_t* next; 
    ty_page_t* prev; 

    // Block lists
    ty_block_t* free; 
    ty_block_t* local_free; 
    _Atomic ty_block_t* thread_free;

    // Metadata 
    size_t used;         // Number of block committed into the free list so far (free + local_free) 
    size_t reserved;     // Number of blocks currently allocated to the user
    size_t capacity;     // The maximum number of blocks that can fit inside this page area
    size_t thread_freed; // Tracks counter for remote free

    size_t block_size;   // Exact byte size, size class of the page
    uint8_t size_bin;    // Index into heap->pages[size_bin]
     
    bool is_full;        // True if page is in TY_BIN_FULL
}; 

struct ty_segment_s {
    uint32_t thread_id;
    size_t page_shift;   // Bit shift exponent 16 for small, 22 for large+ 
    ty_page_kind_t page_kind; 
    size_t segment_size; // for mummap custom segments 

    // Page bookkeeping
    uint8_t capacity;      // Number of pages in this segment 
    uint8_t used;         // Number of page used  /* Refactor: used+capacity can be derived from page_in_use */
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

};


// Function prototypes
ty_segment_t* ty_segment_alloc(ty_heap_t* heap, ty_page_kind_t page_kind, size_t required_size);

ty_heap_t* ty_heap_get_default(void);
size_t ty_os_page_size(void);
uint32_t get_current_thread_id(void);


#endif