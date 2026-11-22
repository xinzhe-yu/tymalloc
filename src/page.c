#include "tymalloc-internal.h"



/* Encapsulates all page state changes 
init extend collect and free are the only ones allowed to manuplate free list pointers
eg. free local_free thread_free*/




ty_page_t* ty_page_carve();        // Growth // Move to internal.h
ty_page_t* ty_page_collect();      // Recycle

ty_page_kind_t ty_page_kind_for_size(size_t block_size);

/* Formates a fresh page within a segment */

void ty_page_init(ty_page_t* page, size_t block_size) {
    
    ty_segment_t* segment = segment_lookup(page);
    int free_slot_index = page_slot_index_lookup(page);

    size_t total_page_size; // Sizes are mentioned in section 3.1 paper
    if (segment->page_kind == TY_PAGE_SMALL) {
        total_page_size = (1ULL << 16); // 64 KiB
    } else if (segment->page_kind == TY_PAGE_LARGE) {
        total_page_size = (1ULL << 22);
    } else if (segment->page_kind == TY_PAGE_HUGE) {
        total_page_size = segment->segment_size;
    } else {
        assert(0 && "ty_page_init: invalid page_kind");
    }

    size_t header_offset = (free_slot_index == 0) ? sizeof(ty_segment_t) : 0;
    header_offset = align_up(header_offset, TY_MAX_ALIGN);

    size_t usable_size = total_page_size - header_offset; 

    page->capacity = usable_size / block_size; 
    page->block_size = block_size; 

  
    // link to same size class?
    //page->next = // Do i have to search through all pages? // When a page is full mark full + make new linked page 
    //page->prev =
   

    uint8_t* page_start;
    if (free_slot_index == 0) {
        page_start = (uint8_t*)((uintptr_t)segment + header_offset);
    } else {
        page_start = (uint8_t*)((uintptr_t)segment + (free_slot_index << segment->page_shift));
    }
    page->page_start = page_start;
  
    page->free = NULL;
    page->reserved = 0;
    page->used = 0;
    page->thread_freed = 0;
    page->is_full = 0; 
    page->local_free = NULL;
    atomic_init(&page->thread_free, NULL);


    // Carve inital batches of pages
    ty_page_carve(page->free);
    
}

/* Carves the next slice of uncarved memory (Inialize blocks by chunk)*/
ty_page_t* ty_page_carve() {
    assert(!"ty_page_extend not implemented");
    return NULL;
}

/* Maintence function ruins when page->free empty AND reserved == Capacity */
/* Collects local_free and thread_free */
ty_page_t* ty_page_collect() {
    assert(!"ty_page_collect not implemented");
    return NULL;
}





// Defined in paper 3.1
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