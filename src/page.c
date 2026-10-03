#include "tymalloc-internal.h"

ty_page_t* ty_page_init();         // Birth  
ty_page_t* ty_page_extend();       // Growth 
ty_page_t* ty_page_collect();      // Recycle
ty_page_t* ty_heap_get_page();     // Retrival

static inline size_t ty_segment_claim_page(ty_segment_t* seg);

/* Formates a fresh page within a segment */
ty_page_t* ty_page_init(ty_segment_t* segment, size_t block_size, size_t free_slot_index) {
    ty_page_t* page = &segment->pages[free_slot_index];
    page->capacity = 
    page->free = 
    page->is_full =
    page->local_free = 
    page->next = 
    page->prev = 
    page->reserved = 
    page->size_class =
    page->thread_free = 
    page->thread_freed = 
    page->used = 
}

/* Carves the next slice of uncarved memory (Inialize blocks by chunk)*/
ty_page_t* ty_page_extend() {

}

/* Maintence function ruins when page->free empty AND reserved == Capacity */
/* Collects local_free and thread_free */
ty_page_t* ty_page_collect() {

}

/* Finds a free page within segment or make a new segment */
ty_page_t* ty_heap_get_page(ty_heap_t* heap, size_t size_class, size_t block_size) {
    ty_page_kind_t required_kind = ty_page_kind_for_size(block_size);
    
    for (ty_segment_t* seg = heap->segments; seg; seg = seg->next) { // search segments to find a unused page 
        if (seg->page_kind == required_kind && seg->used < seg->capacity) {

            size_t free_page_idx = ty_segment_claim_page(seg);

            return ty_page_init(seg, block_size, free_page_idx); // Found a compatible segment with space
        }
    }

    // Make a new segment 
    ty_segment_t* seg = ty_segment_alloc(heap, required_kind, block_size);
    size_t free_page_idx = ty_segment_claim_page(seg);
    return ty_page_init(seg, block_size, free_page_idx); 

}

/* Returns a free_page_index and mark it as used (Vector bookkeeping) */
static inline size_t ty_segment_claim_page(ty_segment_t* seg) {
    uint64_t free_mask = ~(seg->page_in_use);
    size_t free_page_idx = __builtin_ctzll(free_mask);
    seg->page_in_use |= (1ULL << free_page_idx);    // 1ULL 64bits
    seg->used++; 
    return free_page_idx; 
}


ty_page_kind_t ty_page_kind_for_size(size_t block_size) {
    if (block_size <= (1 << 13)) { // 8 KiB
        return TY_PAGE_SMALL; 
    }
    if ((1 << 13) < block_size && block_size <= (1 << 19)) {
        return TY_PAGE_LARGE;
    } 
    if ((1 << 19) < block_size) {
        return TY_PAGE_HUGE;
    }
    return TY_PAGE_HUGE;
}