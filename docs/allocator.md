# Memory Allocator (TLSF)

## Overview
The **soRTOS** memory allocator uses the **TLSF (Two-Level Segregated Fit)** algorithm. It is designed for real-time systems where predictability is critical.

Unlike standard allocators that might search through a long list of free blocks (taking longer as memory gets fragmented), TLSF performs all operations in **O(1)** (constant) time. This means `malloc` and `free` take the same amount of time regardless of the heap size or fragmentation state.

It is fully **thread-safe**, protected by a spinlock to ensure atomic access even in multi-core environments.

## Core Concepts

### 1. The Block Header
Every memory block, whether used or free, starts with a header. This metadata allows the allocator to manage the block and merge it with neighbors when freed.

```c
typedef struct BlockHeader {
    size_t size;                        /* Total size. Bit 0 is the FREE flag. */
    struct BlockHeader *prev_phys_block;/* Pointer to the physical previous block. */
    
    /* Pointers used ONLY when the block is free (stored inside the data area) */
    struct BlockHeader *next_free;
    struct BlockHeader *prev_free;
} block_header_t;
```

*   **`size`**: Stores the size of the block. Since blocks are aligned (e.g., 8 bytes), the least significant bit (LSB) is always 0. We use this bit as the **FREE flag** (1 = Free, 0 = Used).
*   **`prev_phys_block`**: Points to the block immediately preceding this one in memory. This allows O(1) merging with the previous block during `free()`.
*   **`next_free` / `prev_free`**: These pointers exist *inside* the user data area. They are only used when the block is free to link it into the segregated free lists. When allocated, this memory is given to the user.

### 2. Two-Level Segregated Fit
TLSF organizes free blocks into a matrix of linked lists, indexed by **First Level (FL)** and **Second Level (SL)** indices.

*   **First Level (FL):** Categorizes blocks by powers of 2 (Logarithmic).
    *   FL 0: 0-1 bytes (unused)
    *   ...
    *   FL 7: 128-255 bytes
    *   FL 8: 256-511 bytes
*   **Second Level (SL):** Linearly subdivides each FL range.
    *   Defined by `SL_INDEX_COUNT_LOG2` (default 5, meaning 32 subdivisions).
    *   Example: For FL 8 (256-511), the range of 256 bytes is split into 32 lists, each covering an 8-byte range.

### 3. Bitmaps for O(1) Search
To find a free block without searching lists, we use bitmaps:
*   **`fl_bitmap`**: A bit is set if *any* block exists in that FL category.
*   **`sl_bitmap[fl]`**: A bit is set if a block exists in that specific SL subdivision.

Using processor instructions like `CLZ` (Count Leading Zeros) or `CTZ` (Count Trailing Zeros), we can find the optimal free list in constant time.

## Algorithms

### Mapping Size to Indices
To determine which list a block belongs to:
1.  **FL = floor(log2(size))**: Found using MSB index.
2.  **SL**:
    *   **Standard Case:** `(size - 2^FL) / (2^FL / SL_COUNT)`. Extracts the bits immediately following the MSB.
    *   **Small Blocks:** For sizes where `FL < SL_LOG2` (preventing underflow), a simplified mapping is used (e.g., `size >> 1`).

### Allocation (`malloc`)
1.  **Calculate Size:** Add overhead (`sizeof(header)`) and align to architecture boundary.
2.  **Search:**
    *   Calculate required (FL, SL).
    *   Check `sl_bitmap` for a non-empty list at or above the required size.
    *   If the specific SL list is empty, check `fl_bitmap` for the next available FL.
    *   Use bitwise ops to find the *smallest* available block that fits.
3.  **Remove:** Unlink the block from the free list.
4.  **Trim:** If the block is significantly larger than requested, split it.
    *   The remaining part becomes a new free block and is inserted into the appropriate list.
5.  **Return:** Mark as USED and return pointer to payload.

### Deallocation (`free`)
1.  **Mark Free:** Set the FREE bit in the header.
2.  **Coalesce (Merge):**
    *   Check `prev_phys_block`. If it is free, remove it from its list and merge it with the current block.
    *   Check the physically next block (calculated by `current + size`). If it is free, remove it and merge.
3.  **Insert:** Calculate the new (FL, SL) for the merged block and insert it into the list. Update bitmaps.

## Configuration
The allocator is tuned via `config/project_config.h`:
*   **`FL_INDEX_MAX`**: Max supported block size (e.g., 30 for 1GB). Reduce this for small RAM devices to save metadata memory.
*   **`SL_INDEX_COUNT_LOG2`**: Granularity of subdivisions (default 5 = 32 lists per FL).

## Visual Representation

**Physical Memory Layout:**
```text
[ Header | Used Data ] [ Header | Free (next/prev) ] [ Header | Used Data ]
^                      ^                             ^
Block A                Block B                       Block C
(Used)                 (Free)                        (Used)
```

**Logical Free Lists:**
```text
FL_BITMAP: 0010... (FL 2 has blocks)
SL_BITMAP[2]: 0001... (SL 0 has blocks)

blocks[2][0] -> [ Block B ] -> NULL
```