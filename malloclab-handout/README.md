# Segregated-Fit Memory Allocator

A `malloc`/`free`/`realloc` implementation in C using segregated free lists, boundary-tag
coalescing, and a size-aware placement policy. Scores 96/100 on the CS:APP allocator
benchmark at 93% average utilization.

Source: [`mm.c`](mm.c)

## Results

| trace | pattern | utilization |
|---|---|---|
| `amptjp-bal` | real program trace | 98% |
| `cccp-bal` | real program trace | 98% |
| `cp-decl-bal` | real program trace | 98% |
| `expr-bal` | real program trace | 99% |
| `coalescing-bal` | free/merge behavior | 99% |
| `random-bal` | randomized sizes | 89% |
| `random2-bal` | randomized sizes | 86% |
| `binary-bal` | alternating small/large, frees all large, requests larger | 94% |
| `binary2-bal` | same, different size mix | 88% |
| `realloc-bal` | one block grown 4,800× | 99% |
| `realloc2-bal` | mixed grow/shrink | 77% |
| **average** | | **93%** |

Score is 60 points scaled by average utilization (0.60 × 93% ≈ 56) plus 40 for throughput,
capped at reference libc speed. 56 + 40 = 96.

Per-trace throughput is omitted — it varies up to 4× between runs on the same binary.

Progression, measured by rebuilding each milestone commit:

| version | utilization | score |
|---|---|---|
| segregated fit, in-place realloc growth | 71% | 83 |
| realloc path refinements | 78% | 87 |
| further realloc path refinements | 81% | 89 |
| size-segregated split placement | 87% | 92 |
| realloc path reordering | 93% | 96 |

## Block format

4-byte header. Sizes are 8-aligned, so the low three bits carry status: bit 0 = this block
allocated, bit 1 = previous block allocated.

```
                     31            3   2   1   0
                    +---------------+---+---+---+
   header           |     size      | 0 | P | A |
                    +---------------+---+---+---+
   allocated block  |          payload          |
                    +---------------------------+

                    +---------------+---+---+---+
   header           |     size      | 0 | P | 0 |
                    +---------------+---+---+---+
                    |    prev free-list pointer |
   free block       +---------------------------+
                    |    next free-list pointer |
                    +---------------------------+
   footer           |     size      | 0 | P | 0 |
                    +---------------+---+---+---+
```

Allocated blocks carry no footer. Only free blocks do. Backward coalescing reads a
predecessor's footer only when bit 1 says that predecessor is free — and free blocks are
exactly the ones that still have footers. Coalescing stays O(1) and every allocated block
saves 4 bytes.

Payloads are 8-byte aligned. Minimum block is 24 bytes: header + two 8-byte list pointers
+ footer, which is what the block must hold once freed.

## Free lists

Nine doubly linked size classes:

```
[24-31] [32-63] [64-127] [128-255] [256-511] [512-1023] [1024-2047] [2048-4095] [4096+]
```

LIFO insertion, first-fit within a class, scanning upward when a class is exhausted. The
list heads live in the first 72 bytes of the heap rather than in static storage.

## Allocation and freeing

Requests round to `max(24, align8(size + 4))` and search from their own size class upward.
On failure the heap extends by `max(asize, 4096)`. The block is unlinked and split when the
remainder is ≥ 24 bytes.

Freeing clears the allocated bit, writes a footer, and coalesces across four neighbor
cases. Invariant: no two free blocks are ever adjacent.

## Reallocation

Ordered ladder, cheapest first:

1. Size unchanged after rounding — return.
2. Shrink — split, free the tail.
3. Absorb next free block — no copy, no `sbrk`.
4. Extend heap, block already at the epilogue — no copy.
5. Absorb next and extend past it — no copy.
6. Absorb previous — relocates payload.
7. Absorb both neighbors — relocates payload.
8. Absorb both and extend — relocates payload.
9. `malloc` + copy + `free`.

## Optimizations

### Size-segregated placement

Small requests are carved from the low end of a free block, large from the high end, so
similar sizes cluster and space freed by one size class stays contiguous.

Without it, the `binary` traces interleave the two sizes; freeing all the large blocks
leaves every hole fenced between two live small blocks, and none of it coalesces. The heap
was already packed near-optimally — 1,048,664 bytes against a theoretical 1,048,000 — so
the loss was arrangement, not overhead.

| trace | before | after |
|---|---|---|
| `binary-bal` | 55% | 94% |
| `binary2-bal` | 51% | 88% |

### Realloc ordered by relocation, not by `sbrk`

Ranking the ladder above to avoid `sbrk` as long as possible is wrong. Extending the heap
by ~128 bytes is cheap; relocating a block scatters the layout, and the vacated space is
taken by the next allocation, forcing the following growth to relocate again. On
`realloc-bal` that compounded to ~937 MB of `memmove`.

Reordering by whether the block stays put:

| metric | before | after |
|---|---|---|
| utilization | 43% | 99% |
| throughput | ~1,800 Kops | ~226,000 Kops |

The utilization number alone did not localize this. The signal was in the timing: that
trace ran 7.8 ms against ≤ 0.13 ms for every other trace in the suite.

## Heap consistency checker

`mm_checkheap` is compile-time toggled and can run after every operation. It validates:

- payload alignment; header/footer agreement
- no allocated block in a free list
- no two adjacent free blocks
- every free block filed in the size class its size maps to
- `next`/`prev` link agreement in both directions
- prologue and epilogue intact
- no cycles in any free list (slow/fast pointer)

Most defects here were silent corruption — e.g. a stale previous-allocated bit causing a
footer read out of a live payload, producing a wild pointer that crashed several
operations later. Asserting invariants after every operation localizes those; the crash
site does not.

## Limitations

- **32-bit word size.** Headers and footers are 4-byte words, following the assignment's
  setup, which caps a single block at ~4 GB. On a 64-bit platform a 64-bit word would be
  the better choice.
- The 96-byte small/large placement threshold is fitted to this benchmark's size
  distribution. Working range for these traces is 80–120 bytes.
- First-fit rather than best-fit within a class.
- Single-threaded; no locking.

## Build

```sh
make
./mdriver -v -t traces
```

Toggle the heap checker via the `#define checkheap` lines at the top of `mm.c`; leave it
off for throughput runs.

`mm.c` is mine. The driver, memory shim, timing code, and traces are CS:APP course
materials.
