#include "tymalloc-internal.h"


/* Encapsualte SO virtual memory. Handles
    mmap mumap, 4 Mib alignment over allocation, slot bitmasking
    No other file should call mmap (except heap OS bootstrap)
*/



ty_segment_t* ty_segment_create(ty_heap_t* heap, ty_page_kind_t page_kind, size_t required_size) {
    size_t segment_size; 
    size_t page_shift;

    if (page_kind == TY_PAGE_SMALL) {
        segment_size = TY_SEGMENT_SIZE; // 4 MiB
        page_shift = 16;             // 64 KiB per page
    } else if (page_kind == TY_PAGE_LARGE) {
        segment_size = TY_SEGMENT_SIZE; // 4 MiB
        page_shift = 22;             // 4 MiB per page
    } else if (page_kind == TY_PAGE_HUGE) {
        segment_size = align_up(required_size + sizeof(ty_segment_t), TY_SEGMENT_SIZE);
        page_shift = 22; 
    } else {
        assert(0 && "ty_segment_create: invalid page_kind");
        return NULL;
    }

    // MMAP 4MiB allignment 
    size_t alloc_size = segment_size * 2; // Overallocating to guarantee natural 4 MiB alignment 
    void *p = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("segment_alloc: mmap");
        return NULL;  
    }

    uintptr_t raw_addr = (uintptr_t) p;
    uintptr_t aligned_addr = align_up(raw_addr, TY_SEGMENT_SIZE);

    // munmap leading unaligned 
    size_t lead_size = aligned_addr - raw_addr; 
    if (lead_size > 0) {
        munmap(p, lead_size);
    }

    // munmap trailing unaligned
    size_t trail_size = alloc_size - lead_size - segment_size; 
    if (trail_size > 0) {
        munmap((void*)(aligned_addr + segment_size), trail_size);
    }

    ty_segment_t* segment = (ty_segment_t*) aligned_addr;
    segment->segment_size = segment_size;
    segment->page_shift = page_shift;
    //segment->page_amounts = capacity; deleted
    segment->page_kind = page_kind;
    segment->thread_id = heap ? heap->thread_id : 0; 

    segment->page_in_use = 0;

    // Add current segment to Heap->segments bookkeeping
    if (heap != NULL) {
        segment->next = heap->segments;
        heap->segments = segment; 
    } else {
        segment->next = NULL; 
        /* To-do: does this need to be put into orphan list*/
    }


    return segment; 
}





// Marks a slot used & return its page pointer
// Or NULL if segment is full 
ty_page_t* ty_segment_claim_page(ty_segment_t* seg) {
    uint64_t free_mask = ~(seg->page_in_use);

    if (free_mask == 0) { // Segment is full 
        return NULL;
    }

    size_t free_page_idx = __builtin_ctzll(free_mask);  
    seg->page_in_use |= (1ULL << free_page_idx);    // 1ULL 64bits // Mark slot used
    return &seg->pages[free_page_idx];

}

//TODO: ty_segment_free_page: Release a page


















