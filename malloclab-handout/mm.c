// mm.c - Segegated free list memory allocator
// 
// 
// In this approach, a block is allocated by searching through the
// segregated free list until an appropriate size has been found.
// An allocated block consists of header + payload, while free blocks
// consists of header, footer, and pointers for the linked list. 
// Blocks can be split or coalesced to improve utilization. Realloc 
// is implemented with optimization. 
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

static void *extend_heap(size_t words);
static void* find_block(size_t class_index, size_t asize);
static char *split(char *bp, size_t asize);
static void list_insert(char *bp);
static void list_remove(char *bp);
static size_t find_sizeclass(size_t size);
static void *coalesce(void *bp);
static void mm_checkheap(int lineno);
static int loop_detection(char *bp);

team_t team = {
    // Team name
    "TY",
    // First member's full name
    "Tommy Yu",
    // First member's email address
    "tommyyu0326@gmail.com",
    // Second member's full name (leave blank if none)
    "",
    // Second member's email address (leave blank if none)
    ""
};

// Heap consistentcy checker 
//#define checkheap(lineno) mm_checkheap(lineno)
#define checkheap(lineno)

// rounds up to the nearest multiple of ALIGNMENT
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// Basic constants and macros
#define HDR 4
#define FTR 4
#define WSIZE 4 // Word and header/footer size (bytes)
#define DSIZE 8 // Double word size (bytes)
#define NCLASSES 9
#define MINBLOCKSIZE 24
#define ALIGNMENT 8
#define CHUNKSIZE (1<<12) // Extend heap by this amount (bytes)
#define MAX(x, y) ((x) > (y)? (x) : (y))
#define MIN(x, y) ((x) > (y)? (y) : (x))


// Pack a size and allocated bit into a word
#define PACK(size, prealloc, alloc) ((size) | (prealloc << 1) | (alloc))
#define GET(p) (*(unsigned int *) (p))
#define PUT(p, val) (*(unsigned int *) (p) = (val))
#define PUT_PTR(p, ptr) (*(char **)(p) = (char *)(ptr))

// set state bits, take header word
#define SET_ALLOC(p) (GET(p) |= 0x1)
#define CLR_ALLOC(p) (GET(p) &= ~0x1)
#define SET_PRE_ALLOC(p) (GET(p) |= 0x2)
#define CLR_PRE_ALLOC(p) (GET(p) &= ~0x2)

// Read the size and allocated fields from address p (usually header/footer)
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_PRE_ALLOC(p) ((GET(p) & 0x2) >> 1)

// Check allocation bit of contiguous block 
#define PRE_IS_ALLOC(bp) (GET_PRE_ALLOC(HDRP(bp)))
#define NEXT_IS_ALLOC(bp) (GET_ALLOC(HDRP(NEXT_BLKP(bp))))

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// Given block ptr bp, compute address of next and previous blocks
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// Given a block ptr bp, access linked free block
#define PREV(bp) (*((char **)bp))
#define NEXT(bp) (*((char **)(((char *)(bp)) + DSIZE)))

// Compute address of first block in appropriate size class
#define HEADLIST(index) (*((char **)((char *)heap_listp + (index * DSIZE))))

// Global variables
static char *heap_listp = NULL; 
static char *prologue_bp = NULL;

// mm_init - initialize the malloc package.
int mm_init(void) {
    heap_listp = NULL;
    prologue_bp = NULL;
    // Create the initial empty heap
    if ((heap_listp = mem_sbrk(22*WSIZE)) == (void *)-1) { return -1; }
    
    // Size class heads
    PUT_PTR(heap_listp + (0*WSIZE), NULL);            // [24-31]
    PUT_PTR(heap_listp + (2*WSIZE), NULL);            // [32-63]
    PUT_PTR(heap_listp + (4*WSIZE), NULL);            // [64-127]
    PUT_PTR(heap_listp + (6*WSIZE), NULL);            // [128-255]
    PUT_PTR(heap_listp + (8*WSIZE), NULL);            // [256-511]
    PUT_PTR(heap_listp + (10*WSIZE), NULL);           // [512-1023]
    PUT_PTR(heap_listp + (12*WSIZE), NULL);           // [1024-2047]
    PUT_PTR(heap_listp + (14*WSIZE), NULL);           // [2048-4095]
    PUT_PTR(heap_listp + (16*WSIZE), NULL);           // [4096-inf]  
    PUT(heap_listp + (18*WSIZE), 0);              // Alignment padding
    PUT(heap_listp + (19*WSIZE), PACK(DSIZE, 1, 1)); // Prologue header
    PUT(heap_listp + (20*WSIZE), PACK(DSIZE, 1, 1)); // Prologue footer
    PUT(heap_listp + (21*WSIZE), PACK(0, 1, 1));     // Epilogue footer. prev contiguous block allocated

    size_t heads = NCLASSES * DSIZE;
    size_t pad = (heads + WSIZE) % DSIZE ? WSIZE : 0;  // pad so prologue payload is 8-aligned
    prologue_bp = heap_listp + heads + pad;
    checkheap(__LINE__);

    return 0;
}

//return last free block or NULL sbrk failed
//  new free block can coalesce with previous free block 
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    // Allocate an even number of words to maintain alignment
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    //get pre state from old epilouge
    int prev_alloc = GET_PRE_ALLOC(HDRP(bp));

    // Initialize free block header/footer, and epilogue header
    PUT(HDRP(bp), PACK(size, prev_alloc, 0));         // Free block's new header
    PUT(FTRP(bp), PACK(size, prev_alloc, 0));         // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0, 1)); // New epilogue header. Prev cont. block just allocated is free
    
    // Coalesce runs case 1 or 3
    return coalesce(bp);
}

// mm_malloc - Allocate a block by incrementing the brk pointer.
//     Always allocate a block whose size is a multiple of the alignment.
void *mm_malloc(size_t size) {
    //checkheap(__LINE__);
    size_t asize;      // Adjusted block size
    char *bp;
    if (size == 0) { return NULL; } // Ignore spurious requests
    if (size <= MINBLOCKSIZE - HDR) // Adjust block size to include overhead and alignment reqs.
        asize = MINBLOCKSIZE;
    else {
        // at least allocate 4 + size allgined
        size_t min = size + HDR;
        asize = DSIZE * ((min + (DSIZE-1)) / DSIZE); // Round to align
    }
    
    //Determin the size class
    size_t class_index = find_sizeclass(asize);

    // First or best fit search freelist
    // go to the free list and find the free block 

    // return null if can't find block 
    if ((bp = find_block(class_index, asize)) == NULL) { // if cant find block, extend
        size_t extend_size = MAX(asize, CHUNKSIZE);
        if ((bp = extend_heap(extend_size/WSIZE)) == NULL) {
            return NULL;
        }
    }
    
    // call before split
    // split changes size which list remove depends on to find size class
    list_remove(bp);

    // test for split
    size_t csize = GET_SIZE(HDRP(bp)); // block size of 
    size_t diff = csize - asize; // size always >= asize
    if ((diff >= MINBLOCKSIZE)) {
        // split. Next contiguous block = fragment itself
        bp = split(bp, asize);
        SET_ALLOC(HDRP(bp));
        checkheap(__LINE__);
    } else {
        // set next block's prev bits. Next contiguous block = next contiguous block
        SET_PRE_ALLOC(HDRP(NEXT_BLKP(bp)));
        SET_ALLOC(HDRP(bp));
        checkheap(__LINE__);
    }

    // update curr bits
    //SET_ALLOC(HDRP(bp));
    
    return bp;
}

// find free block
// returns pointer to the block 
// maybe do some state updates? -no, only find block 
static void* find_block(size_t class_index, size_t asize) {
    for(;class_index < NCLASSES; class_index++) {
        for (char *bp = HEADLIST(class_index); bp; bp = NEXT(bp)) { //iterate blocks
            if (GET_SIZE(HDRP(bp)) >= asize && !GET_ALLOC(HDRP(bp))) {
                return bp; // found
            }
        }    
    }
    return NULL;
    // this logic took longer than it should to write
}

// split extra block and place fragment in appropriate size class
static char *split(char *bp, size_t asize) {
    char *frag_bp;
    char *new_bp;
    size_t csize = GET_SIZE(HDRP(bp));
    size_t frag_size = csize - asize;

    if (asize < MINBLOCKSIZE) { // split any surplus of 24+ 
        return bp; 
    }

    if (frag_size < MINBLOCKSIZE) {
        return bp;
    }

    if (csize == asize) {
        return bp;
    }
    
    /* split placement policy 
    small placement takes the fornt 
    large request take the back */


    // Score: 92/100 | if (asize < (csize/2)) 
    if (asize < 96) { // asize than half of csize. place alloc at front
        frag_bp = bp + asize;
        // Pack headers/footers
        // Being allocated block - no footer
        PUT(HDRP(bp), PACK(asize, GET_PRE_ALLOC(HDRP(bp)), 1));
        new_bp = bp;
        // Free block / set frag block's prev bits
        PUT(HDRP(frag_bp), PACK(frag_size, 1, 0)); 
        PUT(FTRP(frag_bp), PACK(frag_size, 1, 0));     
        // coalesce frag block 
        
        coalesce(frag_bp);  
        checkheap(__LINE__);
    } 
    else { // frag more than half of csize. place alloc at back 
        frag_bp = bp;
        new_bp = (char *)bp + frag_size;
        memmove(new_bp, bp, (asize - HDR));
        // Alloc header
        PUT(HDRP(new_bp), PACK(asize, 0, 1));
        // Next contiguous block's prev bit is 1
        SET_PRE_ALLOC((HDRP(NEXT_BLKP(new_bp))));
        // Header/footer of frag block, inherit prev bit from bp 
        PUT(HDRP(frag_bp), PACK(frag_size, GET_PRE_ALLOC(HDRP(bp)), 0));
        PUT(FTRP(frag_bp), PACK(frag_size, GET_PRE_ALLOC(HDRP(frag_bp)), 0));
        coalesce(frag_bp);
    }  
    return new_bp;
}

// Insert policy for free block
// Find and inserts free block into data structure
static void list_insert(char *bp) { // LIFO
    // take a free block
    // insert at beginning
    size_t index = find_sizeclass(GET_SIZE(HDRP(bp)));
    char *first = HEADLIST(index); //lands first block

    HEADLIST(index) = bp; // LIFO 
    PREV(bp) = NULL;

    if (first == NULL) { // If size class was empty 
        NEXT(bp) = NULL; // bp->Next = 0;
    } else {
        NEXT(bp) = first;
        PREV(first) = bp;
    }
}

// remove a block from data structure, relink
// remove addr pointing to this block 
static void list_remove(char *bp) {
    char *prev_p = PREV(bp); // land on block bp or Null
    char *next_p = NEXT(bp); // land on block bp or Null
    size_t index = find_sizeclass(GET_SIZE(HDRP(bp)));
    // Case 1 prev null, next null
    if (!prev_p && !next_p) {
        HEADLIST(index) = NULL; // head->next null
    }
    // Case 2 prev 1, next null
    else if (prev_p && !next_p) {
        NEXT(PREV(bp)) = NULL; //bp->prev->next = Null;
    }
    // Case 3 prev null, next 1;
    else if (!prev_p && next_p) {
         // head->next = bp->next;
        HEADLIST(index) = NEXT(bp);
        // bp->next->prev = NULL;
        PREV(NEXT(bp)) = NULL;
    }
    // Case 4 prev 1, next 1
    else if (prev_p && next_p) {
        // bp->prev->next = bp->next;
        NEXT(PREV(bp)) = NEXT(bp);
        // bp->next->prev = bp->prev
        PREV(NEXT(bp)) = PREV(bp);
    }
}


// #define HEADLIST(index) *((char **)((char *)heap_listp + (index * DSIZE)))

// helper to find size class
// returns index of head list
static size_t find_sizeclass(size_t size) {
    /* maybe use bitwise for optimization  */ 
    if (24 <= size && size < 32) { 
        return 0;
    } else if (32 <= size && size < 64) {
        return 1;
    } else if (64 <= size && size < 128) {
        return 2;
    } else if (128 <= size && size < 256) {
        return 3;
    } else if (256 <= size && size < 512) {
        return 4;
    } else if (512 <= size && size < 1024) {
        return 5;
    } else if (1024 <= size && size < 2048) {
        return 6;
    } else if (2048 <= size && size < 4096) {
        return 7;
    } else if (4096 <= size) {
        return 8;
    }
    return -1;
}

// mm_free - Freeing a block does nothing.
void mm_free(void *bp) {
    size_t csize = GET_SIZE(HDRP(bp));
    //change allocation bit
    //give self footer
    CLR_ALLOC(HDRP(bp));
    PUT(FTRP(bp), PACK(csize, GET_PRE_ALLOC(HDRP(bp)), 0));
    //coalesce
    coalesce(bp);
    //checkheap(__LINE__);
}

//returns bp
static void *coalesce(void *bp) {
    // Allocated = No footer
    // Not allocated = Footer
    size_t prev_alloc = PRE_IS_ALLOC(bp);
    size_t next_alloc = NEXT_IS_ALLOC(bp);
    size_t csize = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            // Case 1
        // bp just freed, not in free list
        // notifiy next alloc block, prev is free
        CLR_PRE_ALLOC(HDRP(NEXT_BLKP(bp)));

        // insert coalesce block into new size class
        list_insert(bp);
        //checkheap(__LINE__);
    }
    
    else if (prev_alloc && !next_alloc) {      // Case 2 // Next free
        // bp just freed, not in free list
        // remove next contiguous block from its free list
        char *n_bp = (NEXT_BLKP(bp));
        list_remove(n_bp);

        // coalsece block - Header, size allocate states
        size_t tsize = csize + GET_SIZE(HDRP(n_bp));  // total size
        PUT(HDRP(bp), PACK(tsize, GET_PRE_ALLOC(HDRP(bp)), 0));
        PUT(FTRP(bp), PACK(tsize, GET_PRE_ALLOC(HDRP(bp)), 0));

        // notifiy next alloc block, prev is free 
        // n_bp was already free, next_block prev bit already 0
        //SET_PRE_ALLOC(HDRP(NEXT_BLKP(bp)));
    
        // insert coalesce block into new size class
        list_insert(bp);
        //checkheap(__LINE__);
    }

    else if (!prev_alloc && next_alloc) {      // Case 3 // prev 
        // bp just freed, not in free list
        // remove prev contiguous block from its free list
        char *p_bp = (PREV_BLKP(bp));
        list_remove(p_bp);

        // coalsece block - Header, size allocate states
        size_t tsize = csize + GET_SIZE(HDRP(p_bp));
        PUT(HDRP(p_bp), PACK(tsize, GET_PRE_ALLOC(HDRP(p_bp)), 0));
        PUT(FTRP(p_bp), PACK(tsize, GET_PRE_ALLOC(HDRP(p_bp)), 0));

        // notifiy next alloc block, prev is free
        CLR_PRE_ALLOC(HDRP(NEXT_BLKP(p_bp)));
        // insert coalesce block into new size class
        list_insert(p_bp);
        bp = p_bp;
        //checkheap(__LINE__);
    }

    else {                                     // Case 4 // Both 
        // bp just freed, not in free list
        // remove prev and next contiguous block from its free list
        char *p_bp = (PREV_BLKP(bp));
        list_remove(p_bp);
        char *n_bp = (NEXT_BLKP(bp));
        list_remove(n_bp);
        
        // coalsece block - Header, size allocate states
        size_t tsize = csize + GET_SIZE(HDRP(p_bp)) + GET_SIZE(HDRP(n_bp));
        PUT(HDRP(p_bp), PACK(tsize, GET_PRE_ALLOC(HDRP(p_bp)), 0));
        PUT(FTRP(p_bp), PACK(tsize, GET_PRE_ALLOC(HDRP(p_bp)), 0));

        // notifiy next alloc block, prev is free
        // n_bp was already free, next_block prev bit already 0
        //SET_PRE_ALLOC(HDRP(NEXT_BLKP(p_bp)));

        // insert coalesce block into new size class
        list_insert(p_bp);
        bp = p_bp;
        //checkheap(__LINE__);
    }
    return bp;
}

// mm_realloc - Implemented simply in terms of mm_malloc and mm_free
void *mm_realloc(void *bp, size_t asize) {
    void *new_bp;
    size_t next_alloc = NEXT_IS_ALLOC(bp);
    size_t prev_alloc = PRE_IS_ALLOC(bp);
    size_t csize = GET_SIZE(HDRP(bp));

    if (asize == 0) {
        mm_free(bp);
        return NULL;
    }

    if (asize <= MINBLOCKSIZE - HDR) // Adjust block size to include overhead and alignment reqs.
        asize = MINBLOCKSIZE;
    else {
        // at least allocate 4 + size allgined
        size_t min = asize + HDR;
        asize = DSIZE * ((min + (DSIZE-1)) / DSIZE); // Round to align
    }

    if (asize == csize) {
        checkheap(__LINE__);
        return bp;
    }
    
    // Shrink Case 
    // If new size fits in block, split extra 
    if (asize < csize) {
        int diff = csize - asize;
        if (diff >= MINBLOCKSIZE) {
            // split
            bp = split(bp, asize);
        }
        checkheap(__LINE__);
        return bp;
    } 

    //absorb next, no move, no sbrk 
    if (!next_alloc) {
        char *n_bp = NEXT_BLKP(bp);
        size_t nsize = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        int tsize = csize + nsize;

        if (tsize >= asize) { // absorb next 
            list_remove(n_bp);
            PUT(HDRP(bp), PACK(tsize, GET_PRE_ALLOC(HDRP(bp)), 1));
            new_bp = bp;
            SET_PRE_ALLOC(HDRP(NEXT_BLKP(new_bp))); // after merge, set new next block's prev alloc bit
            int diff = tsize - asize;
            if (diff >= MINBLOCKSIZE) {
                new_bp = split(new_bp, asize); //split extra absorbed free block, extended_heap allocates by 4k chunks
            }
            checkheap(__LINE__);
            return new_bp; 
        } 
    }

    // Extend Heap, no move, sbrk 
    if ((GET_SIZE(HDRP(NEXT_BLKP(bp))) == 0) && GET_ALLOC(HDRP(NEXT_BLKP(bp)))) { // Next block is epilouge, extend heap to realloc 
        int diff = asize - csize;
        char *ex_bp = extend_heap(MAX(diff, MINBLOCKSIZE)/WSIZE);
        if (ex_bp == NULL) {
            return NULL;
        }
        list_remove(ex_bp);
        PUT(HDRP(bp), PACK(csize + GET_SIZE(HDRP(ex_bp)), GET_PRE_ALLOC(HDRP(bp)), 1));
        SET_PRE_ALLOC(HDRP(NEXT_BLKP(bp)));
        checkheap(__LINE__);
        return bp;
    }

    // Absorb next, no move, sbrk
    if (!next_alloc) {
        char *n_bp = NEXT_BLKP(bp);
        size_t nsize = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        int tsize = csize + nsize;

        if (GET_SIZE(HDRP(NEXT_BLKP(n_bp))) == 0 && tsize < asize) { // Free + epilouge = extend heap 
            int diff = asize - tsize;
            char *ex_bp = extend_heap(MAX(diff, MINBLOCKSIZE)/WSIZE); // Next block and extended block coalesced impicitly 
            if (ex_bp == NULL) {
                return NULL;
            }
            tsize = tsize + GET_SIZE(HDRP(ex_bp)) - nsize;
            list_remove(ex_bp);
            PUT(HDRP(bp), PACK(tsize, GET_PRE_ALLOC(HDRP(bp)), 1));
            SET_PRE_ALLOC(HDRP(NEXT_BLKP(bp))); // after merge, set new next block's prev alloc bit
            diff = tsize - asize;
            if (diff >= MINBLOCKSIZE) {
                bp = split(bp, asize); //split extra absorbed free block, extended_heap allocates by 4k chunks
            }
            checkheap(__LINE__);
            return bp;
        }
    }

    // absorb prev, move, no sbrk 
    if (!prev_alloc) {    
        size_t psize = GET_SIZE(HDRP(PREV_BLKP(bp))); 
        size_t tsize = psize + csize;
        char *p_bp = PREV_BLKP(bp);

        if(tsize >= asize) { // see if prev size + csize >= asize 
            list_remove(p_bp); // remove_list(prev)
            new_bp = p_bp;
            PUT(HDRP(new_bp), PACK(tsize, GET_PRE_ALLOC(HDRP(p_bp)), 1));  // pack prev/new header - grab prev-alloc bit from curr header 
            memmove(new_bp, bp, (csize - HDR)); // memmove payload to payload, size of oldsize - HDR
            if (tsize - asize >= MINBLOCKSIZE) {
                new_bp = split(new_bp, asize); //split extra absorbed free block, extended_heap allocates by 4k chunks
            }

            checkheap(__LINE__);
            return new_bp;
        }
    }

    // absorb prev + next, move, no sbrk 
    if (!next_alloc && !prev_alloc) {
        size_t nsize = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        size_t psize = GET_SIZE(HDRP(PREV_BLKP(bp)));
        size_t tsize = nsize + psize + csize;

        char *n_bp = NEXT_BLKP(bp);
        char *p_bp = PREV_BLKP(bp);

        if (tsize >= asize) { // check if size of nsize + psize + csize >= asize 
            list_remove(n_bp); // remove_list next & prev
            list_remove(p_bp); // idea: scan both at once
            PUT(HDRP(p_bp), PACK(tsize, GET_PRE_ALLOC(HDRP(p_bp)), 1)); // pack new/prev header, inherit from p_bp 
            new_bp = p_bp;
            SET_PRE_ALLOC(HDRP(NEXT_BLKP(new_bp))); // after merge, set new next block's prev alloc bit
            memmove(new_bp, bp, (csize - HDR)); // memmove payload to prev's payload
            if (tsize - asize >= MINBLOCKSIZE) {
                new_bp = split(new_bp, asize); //split extra absorbed free block
            }
            checkheap(__LINE__);
            return new_bp;

        }
    }
    
    // absrob prev + next, move, sbrk
    if (!next_alloc && !prev_alloc) {
        size_t nsize = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        size_t psize = GET_SIZE(HDRP(PREV_BLKP(bp)));
        size_t tsize = nsize + psize + csize;

        char *n_bp = NEXT_BLKP(bp);
        char *p_bp = PREV_BLKP(bp);
   
        if (GET_SIZE(HDRP(NEXT_BLKP(n_bp))) == 0 && tsize < asize) { // combine and extend heap
            int diff = asize - tsize;
            char *ex_bp = extend_heap(MAX(diff, MINBLOCKSIZE)/WSIZE); // Next block and extended block coalesced impicitly 
            if (ex_bp == NULL) {
                return NULL;
            }
            tsize = tsize + GET_SIZE(HDRP(ex_bp)) - nsize;
            list_remove(ex_bp);
            list_remove(p_bp);
            PUT(HDRP(p_bp), PACK(tsize, GET_PRE_ALLOC(HDRP(bp)), 1));
            new_bp = p_bp;
            SET_PRE_ALLOC(HDRP(NEXT_BLKP(new_bp))); // after merge, set new next block's prev alloc bit
            memmove(new_bp, bp, (csize - HDR)); // memmove payload to prev's payload
            diff = tsize - asize;
            if (diff >= MINBLOCKSIZE) {
                new_bp = split(new_bp, asize); //split extra absorbed free block, extended_heap allocates by 4k chunks
            }
            checkheap(__LINE__);
            return new_bp; 
        }
    }
    
    // Fallback
    new_bp = mm_malloc(asize + (asize*0)); // 25% slack buffer
    //new_bp = mm_malloc(asize); // 25% slack buffer
    if (new_bp == NULL) {
        return NULL;
    }
    size_t old_payload = GET_SIZE(HDRP(new_bp)) - HDR;
    size_t new_payload = GET_SIZE(HDRP(bp)) - HDR;
    // Min of two payload because possible expand/shrink
    // memcpy starting at payload to payload no header
    memcpy(new_bp, bp, MIN(old_payload, new_payload));
    mm_free(bp);
    checkheap(__LINE__);
    return new_bp;
}

static void mm_checkheap(int lineno) {
    // Free list level
    // For each free block make sure header == footer in size and allocation
    for(int class_index = 0; class_index < NCLASSES - 1; class_index++) {
        for (char *bp = HEADLIST(class_index); bp; bp = NEXT(bp)) { //iterate blocks
            if (GET_SIZE(HDRP(bp)) != GET_SIZE(FTRP(bp))) { // Size mismatch 
                printf("Size missmatch: Line %d", lineno);
            }
            if (GET_ALLOC(HDRP(bp)) != GET_ALLOC(FTRP(bp))) { // Alloc bit mismatch 
                printf("Allocation bit missmatch: Line %d\n", lineno);
            }
            if (GET_ALLOC(HDRP(bp)) == 1 || GET_ALLOC(FTRP(bp)) == 1) { // alloc block in freelist
                printf("Allocated block in freelist: Line %d\n", lineno);
            }
            if ((uintptr_t)bp % 8 != 0) { // Payload area is aligned 
                printf("Payload misaligned: Line %d\n", lineno);
            }
            if (find_sizeclass(GET_SIZE(HDRP(bp))) != class_index) { // Segregated list contain only blocks that beling to the size class
                printf("Free block in wrong size class: Line %d\n", lineno);
            }
            if (NEXT(bp) != NULL) { // Next/prev pointers in consecutive free blocks are consistent
                //check if bp->next->prev == bp 
                if (PREV(NEXT(bp)) != bp) {
                    printf("Next/Prev mismatch: Line %d\n", lineno);
                }
            }
        }    
    }

    // Heap level
    // Prologue/Epilogue blocks are at specific locations (e.g. heap boundaries and have special size/alloc fields)
    // All blocks stay in between the heap boundares

    char *epi_bp = (char *)mem_heap_hi() - 3;
    
    if (GET_SIZE((epi_bp)) != 0) { //Epilogue is at specific locations
        printf("Epilouge misplaced: Line %d\n", lineno);
    }

    if (GET_SIZE((epi_bp)) == 0 && !GET_ALLOC((epi_bp))) { //Epilogue never has free bit
        printf("Epilouge alloc bit = 0, should = 1: Line %d\n", lineno);
    }

    // hardcoded prolouge offsest 
    char *pro_bp = prologue_bp;
    if (GET_SIZE((pro_bp)) != DSIZE) {
        printf("Prolouge header size error: Line %d\n", lineno);
    }
    if (GET_SIZE((((char *)pro_bp) + WSIZE)) != DSIZE) {
        printf("Prolouge footer size error: Line %d\n", lineno);
    }

    char *first_payload = heap_listp + (22*WSIZE);
    //char *first_payload = prologue_bp + (2*WSIZE);
    for (char *bp = first_payload; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (GET_ALLOC(HDRP(bp)) == 0 && GET_ALLOC(HDRP(NEXT_BLKP(bp))) == 0) { // No contigous free blocks in memory
            printf("Consecutive free blocks: Line %d\n", lineno);
        }
        if (GET_ALLOC(HDRP(bp)) != GET_PRE_ALLOC(HDRP(NEXT_BLKP(bp)))) {
            printf("Next block's Prev alloc bit is inncorrectly set: Line %d\n", lineno);
        }

        if (!GET_ALLOC(HDRP(bp))) { // All free blocks are in the free list
            // find in list 
            int is_include = 0;
            for (char *t_bp = HEADLIST(find_sizeclass(GET_SIZE(HDRP(bp)))); t_bp; t_bp = NEXT(t_bp)) { // Travse the size class
                if (bp == t_bp) {
                    is_include = 1;
                    break;
                }
            }
            if (!is_include) {
                printf("Free block not in free list: Line %d\n", lineno);
            }
        }
    }
    // No cycles in the list
    for (int i = 0; i < NCLASSES; i++) { //iterate all heads
        // Pass in head 
        // call cycle detection 
        if(loop_detection(HEADLIST(i))) {
            printf("Free list cycle detected: Line %d\n", lineno);
        }
    }
    // Other 
}

// returns if cycle detected 
static int loop_detection(char *bp) {
    char *slow = bp, *fast = bp;
    while (fast && NEXT(fast)) {
        slow = NEXT(slow);
        fast = NEXT(NEXT(fast));
        if (slow == fast) {
            return 1;
        }
    }
    return 0;
}
