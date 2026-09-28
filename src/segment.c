#include "tymalloc-internal.h"


typedef enum ty_page_kind_e {
    TY_PAGE_SMALL,
    TY_PAGE_LARGE,
    TY_PAGE_HUGE
} ty_page_kind_t;

struct ty_segment_s {
    uint32_t thread_id;
    size_t page_shift;   // Bit shift exponent 16 for small, 22 for large+ 
    ty_page_kind_t page_kind; 
    size_t segment_size; // for mummap custom segments 
    size_t capcity;      // Number of pages in this segment 

    ty_page_t pages[64];
} ty_segment_t; 
 
















