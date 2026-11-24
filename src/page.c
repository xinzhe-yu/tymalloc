#include "tymalloc-internal.h"



/* Encapsulates all page state changes 
init extend collect and free are the only ones allowed to manuplate free list pointers
eg. free local_free thread_free*/




void ty_page_extend();        // Growth // Move to internal.h
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

    // Page 0 is smaller than all other pages
    // Page 0 includes Segment's meta data
    size_t start_offset = page_start_offset(free_slot_index);
    size_t usable_size = total_page_size - start_offset; 
    
    page->capacity = usable_size / block_size; 
    assert(page->capacity != 0);

    

  
    // link to same size class?
    //page->next = // Do i have to search through all pages? // When a page is full mark full + make new linked page 
    //page->prev =
   
    page->block_size = block_size; 
    page->free = NULL;
    page->reserved = 0;
    page->used = 0;
    page->thread_freed = 0;
    page->is_full = 0; 
    page->local_free = NULL;
    atomic_init(&page->thread_free, NULL);


    // Carve inital batches of pages
    ty_page_extend(page);
    
}

/* Carves the next slice of uncarved memory (Inialize blocks by chunk)*/
void ty_page_extend(ty_page_t* page) {
    assert(page->free == NULL);
    size_t reserved = page->reserved;
    if (reserved >= page->capacity) return;

    size_t remaining = page->capacity - reserved;
    size_t max_batch = 64;
    size_t count = MIN(remaining, max_batch);
    size_t bsize = page->block_size; 
    
    uint8_t* payload_base = get_payload_base(page);

    // Payload_start = carve_pointer
    uint8_t* payload_start = payload_base + (reserved * bsize);

    // Make count linked blocks
    ty_block_t head;
    ty_block_t* curr = &head;

    for (int i = 0; i < (int)count; i++) {
        curr->next = (ty_block_t*)payload_start;
        payload_start = (uint8_t*)payload_start + bsize; 
        curr = curr->next;
    }

    page->reserved += count;
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