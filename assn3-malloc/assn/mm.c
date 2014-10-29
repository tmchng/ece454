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
    ""
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
 * Variables for segregated list
 */
#define SL_CLASSES          32
// Overhead: header, ptr, ptr, footer
#define SL_OVERHEAD         WSIZE*4
#define SL_MIN_BLK_SIZE     32
#define SL_NEXT_FREE_BLKP(p) ((char *)(p))
#define SL_PREV_FREE_BLKP(p) ((char *)(p) + WSIZE)
// Indexed by powers of 2
void *sl[SL_CLASSES];

// Segregated list function declarations
void *sl_find_fit(size_t asize);
int sl_get_cl_index_by_size(size_t asize);
void *sl_get_free_blk(int cl_index, size_t asize);
void *sl_extend_heap(size_t size);
void sl_insert(void *bp, int cl_index);


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
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        return (PREV_BLKP(bp));
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
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);

    // Mark block as deallocated
    // coalesce
    // Insert coalesced chunk back to sl
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

    // Add overhead
    // Calculate alligned size
    // Find a fit from SL
    // If no fit found, allocate block for the required size and place it in SL.

    /* Adjust block size to include overhead and alignment reqs. */
    asize = size + SL_OVERHEAD;
    if (asize <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);

    /* Search the free list for a fit */
    if ((bp = sl_find_fit(asize)) != NULL) {
        //place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
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

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    /* Copy the old data. */
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
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
void *sl_find_fit(size_t asize) {
    void *bp;

    // Calculate index of appropriate size class
    int cl_index = sl_get_cl_index_by_size(asize);

    // Get free block from index
    bp = sl_get_free_blk(cl_index, asize);
    if (bp == NULL) {
        // No fit found. Allocate.
        bp = sl_extend_heap(asize);
        sl_insert(bp, cl_index);
    }

    return bp;
}

void *sl_get_free_blk(int cl_index, size_t asize) {
    // Get the first block that fits
    // SL is ordered by size, from small to large.
    void *ptr = sl[cl_index];

    while (ptr) {
        if (GET_SIZE(ptr) >= asize) {
            return ptr;
        } else {
            ptr = (void *) GET(SL_NEXT_FREE_BLKP(ptr));
        }
    }
    return NULL;
}


void sl_insert(void *bp, int cl_index) {
    // Assume that cl_index is the correct one for bp size.
    // Place in ascending order of size
    assert(cl_index < SL_CLASSES);
    if (!bp) return;

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
        if (GET_SIZE(bp) >= GET_SIZE(ptr)) {
            prev = ptr;
            ptr = (void *) GET(SL_NEXT_FREE_BLKP(bp));
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

void *sl_extend_heap(size_t size) {
    assert(size % DSIZE == 0);

    void *bp;

    if ( (bp = mem_sbrk(size)) == (void *)-1 ) {
        return NULL;
    }

    // Initialize free block header, footer, and epilogue
    PUT(HDRP(bp), PACK(size, 0));   // free block header

    PUT(SL_NEXT_FREE_BLKP(bp), 0);  // NEXT ptr
    PUT(SL_PREV_FREE_BLKP(bp), 0);  // PREV ptr

    PUT(FTRP(bp), PACK(size, 0));   // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header

    // Coalesce if the previous block was free
    return coalesce(bp);
}

int sl_get_cl_index_by_size(size_t asize) {
    assert(asize % DSIZE == 0);
    assert(asize > SL_MIN_BLK_SIZE);

    asize = asize >> 6;
    int index = 0;

    while (asize > 0) {
        if (index >= SL_CLASSES-1) return index; // at largest class
        index++;
        asize = asize >> 1;
    }
    return index;
}
