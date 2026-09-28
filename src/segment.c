#include "tymalloc-internal.h"

#include <sys/mman.h> 

#define TY_PAGE_SIZE (1 << 22)

 ty_segment_t* ty_segment_alloc(ty_page_kind_t page_kind, size_t required_size) {
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



    void *p = mmap(NULL, segment_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("segment_alloc: mmap");
        return NULL;  
    }

    ty_segment_t* segment = (ty_segment_t*) p;
    segment->segment_size = segment_size;
    segment->page_shift = page_shift;
    segment->capcity = capacity;
    segment->page_kind = page_kind;
    segment->thread_id = get_current_thread_id();
    
    return segment; 
 }
















