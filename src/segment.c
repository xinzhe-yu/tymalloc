#include "tymalloc-internal.h"

#include <sys/mman.h> 

#define TY_PAGE_SIZE (1 << 22)

static inline size_t align_up(size_t size, size_t alignment);

 ty_segment_t* ty_segment_alloc(ty_heap_t* heap, ty_page_kind_t page_kind, size_t required_size) {
    size_t segment_size; 
    size_t page_shift;
    size_t capacity;

    if (page_kind == TY_PAGE_SMALL) {
        segment_size = TY_PAGE_SIZE; // 4 MiB
        page_shift = 16;             // 64 KiB per page
        capacity = 64;
    } else if (page_kind == TY_PAGE_LARGE) {
        segment_size = TY_PAGE_SIZE; // 4 MiB
        page_shift = 22;             // 4 MiB per page
        capacity = 1;
    } else if (page_kind == TY_PAGE_HUGE) {
        segment_size = align_up(required_size + sizeof(ty_segment_t), TY_PAGE_SIZE);
        page_shift = 22; 
        capacity = 1; 
    }

    // MMAP 4MiB allignment 
    size_t alloc_size = segment_size * 2; 
    void *p = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("segment_alloc: mmap");
        return NULL;  
    }

    uintptr_t raw_addr = (uintptr_t) p;
    uintptr_t aligned_addr = align_up(raw_addr, TY_PAGE_SIZE);

    // Unmap leading unaligned 
    size_t lead_size = aligned_addr - raw_addr; 
    if (lead_size > 0) {
        munmap(p, lead_size);
    }

    // Unmap trailing unaligned
    size_t trail_size = alloc_size - lead_size - segment_size; 
    if (trail_size > 0) {
        munmap((void*)(aligned_addr + segment_size), trail_size);
    }

    ty_segment_t* segment = (ty_segment_t*) p;
    segment->segment_size = segment_size;
    segment->page_shift = page_shift;
    segment->capcity = capacity;
    segment->page_kind = page_kind;
    segment->thread_id = heap ? heap->thread_id : 0; 

    // Heap->segments keeps track of all active segments
    if (heap != NULL) {
        segment->next = heap->segments;
        heap->segments = segment; 
    } else {
        segment->next = NULL; 
    }
    
    return segment; 
 }


 static inline size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
 }

















