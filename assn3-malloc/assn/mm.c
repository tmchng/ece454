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

//#define DEBUG 1

#ifdef DEBUG
    #define DBG_ASSERT(arg) assert(arg)
    #define DBG_PRINT(...) printf(__VA_ARGS__)
#else
    #define DBG_ASSERT(arg)
    #define DBG_PRINT(...)
#endif

/**
 * LAB3
 * Variables for segregated list
 */
#define SL_CLASSES          32
// Overhead: header, ptr, ptr, footer
#define SL_OVERHEAD         DSIZE
#define SL_MIN_BLK_SIZE     2 * DSIZE
#define SL_NEXT_FREE_BLKP(p) ((char *)(p))
#define SL_PREV_FREE_BLKP(p) ((char *)(p) + WSIZE)
// Indexed by powers of 2
void *sl[SL_CLASSES];

// Segregated list function declarations
void *sl_find_fit(size_t asize);
int sl_get_cl_index_by_size(size_t asize);
void sl_insert(void *bp);
void sl_crop_unused(void *bp, size_t asize);
void sl_remove(void *bp);
void sl_place(void *bp);
void sl_init();
void sl_print();
void sl_insert_head(void *bp);
void sl_insert_ordered(void *bp);
size_t sl_get_asize(size_t size);


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

    DBG_PRINT("coalesce 2 newsize=%u at %lx\n", (unsigned int)size, (uintptr_t)bp);
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        sl_remove(prev);

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(prev), PACK(size, 0));
    DBG_PRINT("coalesce 3 newsize=%u at %lx\n", (unsigned int)size, (uintptr_t)bp);
        return (prev);
    }

    else {            /* Case 4 */
        sl_remove(prev);
        sl_remove(next);

        size += GET_SIZE(HDRP(prev)) + GET_SIZE(HDRP(next))  ;
        PUT(HDRP(prev), PACK(size,0));
        PUT(FTRP(next), PACK(size,0));
    DBG_PRINT("coalesce 4 newsize=%u at %lx\n", (unsigned int)size, (uintptr_t)bp);
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

    DBG_PRINT("extend heap size = %u\n", (unsigned int)size);
    DBG_ASSERT(size > 0);
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
    DBG_PRINT("- free %lx size=%u\n", (uintptr_t) bp, (unsigned int)size);
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    sl_insert(coalesce(bp));
}


/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(..)
 * If no block satisfies the request, the heap is extended
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

    DBG_PRINT("- malloc start asize=%u, size=%u\n", (unsigned int)asize, (unsigned int)size);

    /* Search the free list for a fit */
    if ((bp = sl_find_fit(asize)) != NULL) {
        sl_place(bp);
        DBG_PRINT("malloc success bsize=%u at %lx\n", (unsigned int)asize, (uintptr_t)bp);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    // Try to split and only get what we need.
    sl_crop_unused(bp, asize);
    sl_place(bp);
    DBG_PRINT("malloc success bsize=%u\n at %lx\n", (unsigned int)asize, (uintptr_t)bp);
    return bp;

}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
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


    DBG_PRINT("- Realloc size=%u at %lx\n", (unsigned int)size, (uintptr_t)ptr);
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
        DBG_ASSERT(GET_ALLOC(HDRP(cptr)) == 0);
        newptr = cptr;
    } else {
        // Have to get a block via malloc
        // Put the coalesced/free block into SL.
        DBG_PRINT("coalesce didn't give us a fit\n");
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
  return 1;
}


/** Segregated list helper functions **/
void sl_init() {
    int i;
    for (i = 0; i < SL_CLASSES; i++) {
        sl[i] = NULL;
    }
}

size_t sl_get_asize(size_t size) {
    // Get the appropriate adjusted size
    size_t asize = size + SL_OVERHEAD;
    if (asize <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
    return asize;
}

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
        DBG_PRINT("cl=%d, size=%lu, alloc=%lu at %lx\n", i,  GET_SIZE(HDRP(ptr)), GET_ALLOC(HDRP(ptr)), (uintptr_t)ptr);
        DBG_ASSERT(GET_ALLOC(HDRP(ptr)) == 0);
        // Remove block from SL
        sl_remove(ptr);
        // Try to split
        sl_crop_unused(ptr, asize);
    }

    return ptr;
}


void sl_crop_unused(void *bp, size_t asize) {
    // Returns the block that fits asize
    // Inserts the free block to sl
    if (!bp) return;
    DBG_ASSERT(GET_ALLOC(HDRP(bp)) == 0);

    size_t bsize = GET_SIZE(HDRP(bp));
    size_t free_size = bsize - asize;
    char alloc = GET_ALLOC(HDRP(bp));
    DBG_ASSERT(free_size >= 0);

    if (free_size < SL_MIN_BLK_SIZE) {
        // Cannot break up the block.
        DBG_PRINT("no crop\n");
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

void sl_remove(void *bp) {
    // Remove a block from SL
    if (!bp) return;
    void *prev = (void*) GET(SL_PREV_FREE_BLKP(bp));
    void *next = (void*) GET(SL_NEXT_FREE_BLKP(bp));
    DBG_ASSERT(GET_ALLOC(HDRP(bp)) == 0);

    DBG_PRINT("remove %lx - prev=%lx, next=%lx", (uintptr_t)bp, (uintptr_t)prev, (uintptr_t) next);

    if (!prev) {
        DBG_PRINT("at head\n");
        // First block in the list
        int cl_index = sl_get_cl_index_by_size(GET_SIZE(HDRP(bp)));
        DBG_ASSERT(sl[cl_index] == bp);
        if (next) {
            PUT(SL_PREV_FREE_BLKP(next), (uintptr_t) prev);
        }
        sl[cl_index] = next;

    } else if (!next) {
        DBG_PRINT("at end\n");
        // Last block in the list
        PUT(SL_NEXT_FREE_BLKP(prev), 0);
    } else {
        DBG_PRINT("in middle\n");
        // Middle block
        // Link next and prev
        PUT(SL_NEXT_FREE_BLKP(prev), (uintptr_t) next);
        PUT(SL_PREV_FREE_BLKP(next), (uintptr_t) prev);
    }
}

void sl_insert(void *bp) {
    sl_insert_ordered(bp);
}

void sl_insert_head(void *bp) {
    if (!bp) return;
    DBG_ASSERT(GET_ALLOC(HDRP(bp)) == 0);

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

void sl_insert_ordered(void *bp) {
    // Place in descending order of size
    if (!bp) return;
    DBG_ASSERT(GET_ALLOC(HDRP(bp)) == 0);

    size_t bsize = GET_SIZE(HDRP(bp));
    DBG_PRINT("sl_insert size=%u at %lx -- ", (unsigned int)bsize, (uintptr_t)bp);
    int cl_index = sl_get_cl_index_by_size(bsize);
    void *ptr = sl[cl_index];
    void *prev = NULL;

    if (!ptr) {
        DBG_PRINT("first one\n");
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
            DBG_PRINT("ptr=%lx\n", (uintptr_t)ptr);
        } else {
            break;
        }
    }

    if (ptr == sl[cl_index]) {
        DBG_PRINT("at head\n");
        // Replacing first in the list
        PUT(SL_NEXT_FREE_BLKP(bp), (uintptr_t) ptr);
        PUT(SL_PREV_FREE_BLKP(bp), 0);
        PUT(SL_PREV_FREE_BLKP(ptr), (uintptr_t) bp);
        sl[cl_index] = bp;

    } else if (!ptr) {
        DBG_PRINT("at last %lx\n", (uintptr_t)ptr);
        // Last in the list
        PUT(SL_NEXT_FREE_BLKP(prev), (uintptr_t) bp);
        PUT(SL_PREV_FREE_BLKP(bp), (uintptr_t) prev);
        PUT(SL_NEXT_FREE_BLKP(bp), 0);
    } else {
        DBG_PRINT("in middle %lx\n", (uintptr_t)ptr);
        // In the middle of list, in front of ptr
        PUT(SL_NEXT_FREE_BLKP(prev), (uintptr_t) bp);
        PUT(SL_PREV_FREE_BLKP(bp), (uintptr_t) prev);
        PUT(SL_NEXT_FREE_BLKP(bp), (uintptr_t) ptr);
        PUT(SL_PREV_FREE_BLKP(ptr), (uintptr_t) bp);
    }
}

int sl_get_cl_index_by_size(size_t asize) {
    DBG_ASSERT(asize % DSIZE == 0);
    DBG_ASSERT(asize >= SL_MIN_BLK_SIZE);

    asize = asize >> 6;
    int index = 0;

    while (asize > 0) {
        if (index >= SL_CLASSES-1) return index; // at largest class
        index++;
        asize = asize >> 1;
    }
    return index;
}

void sl_place(void *bp) {
  size_t bsize = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(bsize, 1));
  PUT(FTRP(bp), PACK(bsize, 1));
}

void sl_print() {
    DBG_PRINT("\n *** current SL ***\n");
    int i;
    void *ptr = NULL;
    for (i = 0; i < SL_CLASSES; i++) {
        DBG_PRINT("-- class %d\n", i);
        ptr = sl[i];
        while (ptr) {
            DBG_PRINT("%lx\n", (uintptr_t)ptr);
            ptr = (void *) GET(SL_NEXT_FREE_BLKP(ptr));
        }
        DBG_PRINT("-- class %d\n", i);
    }
    DBG_PRINT("\n");
}
