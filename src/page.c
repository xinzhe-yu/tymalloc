#include "tymalloc-internal.h"





ty_page_t ty_page_fresh();         // Birth  
ty_page_t ty_page_extend();       // Growth 
ty_page_t ty_page_collect();      // Recycle
ty_page_t ty_heap_get_page();     // Retrival



ty_page_t ty_page_fresh() {

}

ty_page_t ty_page_extend() {

}

ty_page_t ty_page_collect() {

}

ty_page_t ty_heap_get_page(ty_heap_t* heap, bin, size_t block_size) {
    ty_page_kind_t required_kind = ty_page_kind_for_size(block_size);
    // search segemts to find a unused page 
    for (ty_segment_t* seg = heap->segments; seg; seg = seg->next) {
        if (seg->page_kind == required_kind && seg->used < seg->capcity) {
            // Found a compatible segment with space
            return ty_page_fresh();
        }
    }
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
}