#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <sys/mman.h>

// Include your header or implementations here
#include "tymalloc.h"
#include "tymalloc-internal.h"

void test_alignment_math(void) {
    printf("[TEST] Testing alignment math...\n");
    assert(align_up(0, 8) == 0);
    assert(align_up(1, 8) == 8);
    assert(align_up(8, 8) == 8);
    assert(align_up(9, 8) == 16);
    assert(align_up(65000, 4096) == 65536);
    printf("  -> Alignment math PASSED!\n");
}

void test_direct_bin_index(void) {
    printf("[TEST] Testing direct bin index lookup...\n");
    assert(((1 + 7) >> 3) == 1);
    assert(((8 + 7) >> 3) == 1);
    assert(((9 + 7) >> 3) == 2);
    assert(((1024 + 7) >> 3) == 128);
    printf("  -> Direct bin index PASSED!\n");
}

int main(void) {
    test_alignment_math();
    test_direct_bin_index();
    // Next: Add test_segment_create() here!
    printf("\nAll tests passed successfully!\n");
    return 0;
}