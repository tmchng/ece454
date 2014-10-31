/*
 * This implementation replicates the implicit list implementation
 * provided in the textbook
 * "Computer Systems - A Programmer's Perspective"
 * Blocks are never coalesced or reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "TeamTW",
    /* First member's full name */
    "Ting-Hao Cheng",
    /* First member's email address */
    "chengti2@ecf.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "Corey Yen",
    /* Second member's email address (leave blank if none) */
    "yenyung1@ecf.utoronto.ca"
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


/**
 * LAB3
 * Segregated List
 *
 * The segrelated list has 32 classes, going from the 32 byte
 * class and up for every power of 2. It uses 8 * 32 = 256 bytes of memory.
 */

#define SL_CLASSES          32
#define SL_OVERHEAD         DSIZE // header + footer
#define SL_MIN_BLK_SIZE     2 * DSIZE // header + footer + 2 ptrs
#define SL_NEXT_FREE_BLKP(p) ((char *)(p))
#define SL_PREV_FREE_BLKP(p) ((char *)(p) + WSIZE)

void *sl[SL_CLASSES];

// Segregated list function declarations
void *sl_find_fit(size_t asize);
int sl_get_cl_index_by_size(size_t asize);
void sl_insert(void *bp);
void sl_crop_unused(void *bp, size_t asize);
void sl_remove(void *bp);
void sl_place(void *bp);
void sl_init();
void sl_insert_head(void *bp);
void sl_insert_ordered(void *bp);
size_t sl_get_asize(size_t size);
void *sl_split_optimize(void *bp, size_t asize);


void* heap_listp = NULL;

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
 int mm_init(void)
 {
   if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
         return -1;
     PUT(heap_listp, 0);                         // alignment padding
     PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));   // prologue header
     PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));   // prologue footer
     PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    // epilogue header
     heap_listp += DSIZE;

     sl_init();

     return 0;
 }

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 *
 * Coalesced block is removed from the segregated list.
 **********************************************************/
void *coalesce(void *bp)
{
    void *prev = (void *) PREV_BLKP(bp);
    void *next = (void *) NEXT_BLKP(bp);

    size_t prev_alloc = GET_ALLOC(FTRP(prev));
    size_t next_alloc = GET_ALLOC(HDRP(next));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {       /* Case 1 */
        return (bp);
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        sl_remove(next);

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        sl_remove(prev);

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(prev), PACK(size, 0));
        return (prev);
    }

    else {            /* Case 4 */
        sl_remove(prev);
        sl_remove(next);

        size += GET_SIZE(HDRP(prev)) + GET_SIZE(HDRP(next))  ;
        PUT(HDRP(prev), PACK(size,0));
        PUT(FTRP(next), PACK(size,0));
        return (prev);
    }
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}


/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * find_fit(size_t asize)
{
    // Unused
    void *bp;
    bp = sl_find_fit(asize);

    return bp;
}

/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t asize)
{
    // Unused
    /* Get the current block size */
    size_t bsize = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(bsize, 1));
    PUT(FTRP(bp), PACK(bsize, 1));
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    if(bp == NULL){
      return;
    }


    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    sl_insert(coalesce(bp));
}


/**********************************************************
 * mm_malloc
 * Allocate a block that can fit size bytes.
 * Extend the heap if there's no available free block.
 **********************************************************/
void *mm_malloc(size_t size)
{
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size <= 0)
        return NULL;


    /* Adjust block size to include overhead and alignment reqs. */
    asize = sl_get_asize(size);


    /* Search the free list for a fit */
    if ((bp = sl_find_fit(asize)) != NULL) {
        sl_place(bp);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    // Try to split and only get what we need.
    bp = sl_split_optimize(bp, asize);
    sl_place(bp);
    return bp;

}

/**********************************************************
 * mm_realloc
 *
 * Reallocate space given pointer to an existing block.
 *
 * Optimized for performance and use of space by avoiding
 * going through the segregated list and allocating new memory
 * block whenever possible.
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));


    void *oldptr = ptr;
    void *newptr;
    void *cptr; // coalesce
    size_t asize = sl_get_asize(size);
    size_t old_size = GET_SIZE(HDRP(oldptr));
    size_t copySize;


    if (asize <= old_size) {
        // Use the old block.
        sl_crop_unused(oldptr, asize);
        return oldptr;
    }

    // Mark old block as free
    PUT(HDRP(oldptr), PACK(old_size,0));
    PUT(FTRP(oldptr), PACK(old_size,0));

    // try to coalesce and get a bigger block
    cptr = coalesce(ptr);

    if (GET_SIZE(HDRP(cptr)) >= asize) {
        // New coalesced block is big enough. Use it.
        newptr = cptr;
    } else {
        // Have to get a block via malloc
        // Put the coalesced/free block into SL.
        newptr = mm_malloc(size);
    }

    if (newptr == NULL) {
        sl_insert(cptr);
        return NULL;
    }

    copySize = old_size;
    if (size < copySize)
        copySize = size;

    if (newptr == cptr) {
        // Using the coalesced block
        if (cptr != oldptr) {
            // Different start addr. Need to move memory.
            memmove(newptr, oldptr, copySize);
        }
        sl_crop_unused(newptr, asize);
        sl_place(newptr);
    } else {
        memcpy(newptr, oldptr, copySize);
        // Put unused block in SL
        sl_insert(cptr);
    }

    return newptr;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){
    unsigned int free_blk_count = 0;
    unsigned int alloc_blk_count = 0;

    size_t free_size = 0;
    size_t alloc_size = 0;
    void *ptr;

    int odd_free_blk = 0;

    // Do a sweep and collect stats on the current heap.


    // Check that all blocks in the free list are not allocated
    // Check that the number of free blocks counted from sweep
    // equals the number of blocks in the segregated list


    // Check that all blocks are either free or allocated
    // by adding the size of used blocks and the size of free blocks.

    // Check that all free blocks on heap can be reached from segregated list
}


/** Segregated list helper functions **/

/**
 * Initialize the Segregated list
 */
void sl_init() {
    int i;
    for (i = 0; i < SL_CLASSES; i++) {
        sl[i] = NULL;
    }
}

/**
 * Computes the aligned size, including overhead.
 *
 * size: the requested block size.
 * return: aligned size
 */
size_t sl_get_asize(size_t size) {
    // Get the appropriate adjusted size
    size_t asize = size + SL_OVERHEAD;
    if (asize <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
    return asize;
}

/**
 * Finds a fitting free block from the segregated list.
 * Splits the block if possible to conserve memory.
 *
 * asize: aligned size
 * return: pointer to free block or NULL
 */
void *sl_find_fit(size_t asize) {
    void *ptr = NULL;
    int i;
    int cl_index = sl_get_cl_index_by_size(asize);

    // Get the first block that fits
    // Go through every class including and above
    // the best bitting class.
    for (i = cl_index; i < SL_CLASSES; i++) {
        ptr = sl[i];

        // Only look at the first one since SL is in descending order.
        if (ptr && GET_SIZE(HDRP(ptr)) >= asize) {
            break;
        }
    }

    if (ptr) {
        // Remove block from SL
        sl_remove(ptr);
        // Try to split
        ptr = sl_split_optimize(ptr, asize);
    }

    return ptr;
}

/**
 * Frees unused chunk of a given block if possible.
 * Free block is reinserted into the segregated list.
 *
 * bp: pointer to a free block
 * asize: aligned size
 */
void sl_crop_unused(void *bp, size_t asize) {
    // Returns the block that fits asize
    // Inserts the free block to sl
    if (!bp) return;

    size_t bsize = GET_SIZE(HDRP(bp));
    size_t free_size = bsize - asize;
    char alloc = GET_ALLOC(HDRP(bp));

    if (free_size < SL_MIN_BLK_SIZE) {
        // Cannot break up the block.
        return;
    }

    // TODO: Allocate towards the similar sized block
    // for smaller fragmentation

    void *free_bp = (void *)(bp + asize);
    // Write header and footer
    PUT(HDRP(free_bp), PACK(free_size, 0));
    PUT(FTRP(free_bp), PACK(free_size, 0));
    // Insert free block back to SL
    sl_insert(free_bp);

    // Update the header and footer for bp
    PUT(HDRP(bp), PACK(asize, alloc));
    PUT(FTRP(bp), PACK(asize, alloc));
}

/**
 * Frees unused chunk of a block if possible, and allocate the used
 * chunk towards the adjacent block with similar size.
 * By grouping similar sized block together, we can reduce external
 * fragmentation.
 *
 * bp: pointer to a block
 * asize; aligned size
 * return: pointer to the asize portion of the block.
 */
void *sl_split_optimize(void *bp, size_t asize) {
    if (!bp) return NULL;

    size_t bsize = GET_SIZE(HDRP(bp));
    size_t free_size = bsize - asize;
    char alloc = GET_ALLOC(HDRP(bp));

    if (free_size < SL_MIN_BLK_SIZE) {
        // Cannot break up the block.
        return bp;
    }

    // Allocate towards the same sized blocks
    void *prev = PREV_BLKP(bp);
    void *next = NEXT_BLKP(bp);
    void *free_bp;
    int size_diff_prev = abs(asize - GET_SIZE(HDRP(prev)));
    int size_diff_next = abs(asize - GET_SIZE(HDRP(next)));

    if (size_diff_prev < size_diff_next) {
        // Place block towards previous block
        free_bp = (void *)(bp + asize);

    } else {
        // Place block towards next block
        free_bp = bp;
        bp = (void *)(bp + free_size);
    }

    // Write header and footer of free block
    PUT(HDRP(free_bp), PACK(free_size, 0));
    PUT(FTRP(free_bp), PACK(free_size, 0));
    // Insert free block back to SL
    sl_insert(free_bp);

    // Update the header and footer for bp
    PUT(HDRP(bp), PACK(asize, alloc));
    PUT(FTRP(bp), PACK(asize, alloc));

    return bp;
}

/**
 * Removes a block from the segregated list.
 *
 * bp: block ptr
 */
void sl_remove(void *bp) {
    // Remove a block from SL
    if (!bp) return;
    void *prev = (void*) GET(SL_PREV_FREE_BLKP(bp));
    void *next = (void*) GET(SL_NEXT_FREE_BLKP(bp));


    if (!prev) {
        // First block in the list
        int cl_index = sl_get_cl_index_by_size(GET_SIZE(HDRP(bp)));
        if (next) {
            PUT(SL_PREV_FREE_BLKP(next), (uintptr_t) prev);
        }
        sl[cl_index] = next;

    } else if (!next) {
        // Last block in the list
        PUT(SL_NEXT_FREE_BLKP(prev), 0);
    } else {
        // Middle block
        // Link next and prev
        PUT(SL_NEXT_FREE_BLKP(prev), (uintptr_t) next);
        PUT(SL_PREV_FREE_BLKP(next), (uintptr_t) prev);
    }
}

/**
 * Inserts a block into the segregated list.
 * The block is placed in appropriate class based on its size.
 *
 * bp: block ptr
 */
void sl_insert(void *bp) {
    sl_insert_ordered(bp);
}

/**
 * Inserts a block into the segregated list.
 * This version always place the block at the head of its class.
 *
 * bp: block ptr
 */
void sl_insert_head(void *bp) {
    if (!bp) return;

    // Always insert at the head of list
    int cl_index = sl_get_cl_index_by_size(GET_SIZE(HDRP(bp)));
    void *next = sl[cl_index];
    if (next) {
        PUT(SL_NEXT_FREE_BLKP(bp), (uintptr_t)next);
        PUT(SL_PREV_FREE_BLKP(next), (uintptr_t)bp);
    } else {
        PUT(SL_NEXT_FREE_BLKP(bp), 0);
    }
    PUT(SL_PREV_FREE_BLKP(bp), 0);
    sl[cl_index] = bp;
}

/**
 * Inserts a block into the segregated list.
 * This version inserts the larger block in front.
 *
 * bp: block ptr
 */
void sl_insert_ordered(void *bp) {
    // Place in descending order of size
    if (!bp) return;

    size_t bsize = GET_SIZE(HDRP(bp));
    int cl_index = sl_get_cl_index_by_size(bsize);
    void *ptr = sl[cl_index];
    void *prev = NULL;

    if (!ptr) {
        // First in the list.
        PUT(SL_NEXT_FREE_BLKP(bp), 0);
        PUT(SL_PREV_FREE_BLKP(bp), 0);
        sl[cl_index] = bp;
        return;
    }

    while (ptr) {
        if (GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(ptr))) {
            prev = ptr;
            ptr = (void *) GET(SL_NEXT_FREE_BLKP(ptr));
        } else {
            break;
        }
    }

    if (ptr == sl[cl_index]) {
        // Replacing first in the list
        PUT(SL_NEXT_FREE_BLKP(bp), (uintptr_t) ptr);
        PUT(SL_PREV_FREE_BLKP(bp), 0);
        PUT(SL_PREV_FREE_BLKP(ptr), (uintptr_t) bp);
        sl[cl_index] = bp;

    } else if (!ptr) {
        // Last in the list
        PUT(SL_NEXT_FREE_BLKP(prev), (uintptr_t) bp);
        PUT(SL_PREV_FREE_BLKP(bp), (uintptr_t) prev);
        PUT(SL_NEXT_FREE_BLKP(bp), 0);
    } else {
        // In the middle of list, in front of ptr
        PUT(SL_NEXT_FREE_BLKP(prev), (uintptr_t) bp);
        PUT(SL_PREV_FREE_BLKP(bp), (uintptr_t) prev);
        PUT(SL_NEXT_FREE_BLKP(bp), (uintptr_t) ptr);
        PUT(SL_PREV_FREE_BLKP(ptr), (uintptr_t) bp);
    }
}

/**
 * Given a size, computes the index to the appropriate class in
 * segregated list.
 *
 * asize: aligned size
 * return: index to the corresponding size class.
 */
int sl_get_cl_index_by_size(size_t asize) {
    asize = asize >> 6;
    int index = 0;

    while (asize > 0) {
        if (index >= SL_CLASSES-1) return index; // at largest class
        index++;
        asize = asize >> 1;
    }
    return index;
}

/**
 * Marks a block as allocated.
 *
 * bp: block ptr
 */
void sl_place(void *bp) {
  size_t bsize = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(bsize, 1));
  PUT(FTRP(bp), PACK(bsize, 1));
}
