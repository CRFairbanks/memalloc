/*
 * mm.c
 *
 * Name: [Matthew Carraway] x [Cody Fairbanks]
 *
 * 
 * DETAILED DESCRIPTION OF APPROACH:
 * We use a fake header-footer to initialize from to begin, and then our allocation just allocates space without really checking for 
 * any data optimization or anything, just ensures allocation by extending the heap by the appropriate amount
 * Our free function just marks the given pointer to free (adds to free list for use in the future) and, with the help
 * of the combine function, maximizes the free space by getting rid of unneccesary block structs and combining all of the 
 * adjacent free list blocks
 * our realloc looks for a free block of appropriate size that exists within the list. If it doesnt exist, it extends the heap appropriately,
 * If it does exist, then it splits (if needed) and memcpys the old information to the found space.
 * our split function takes a free space in memory thats too big for the space we need, and it splits it into one of appropriate size and one
 * of leftover free space. 
 * 
 * We didn't implement a very efficient code yet, but pass 40/100 of the final traces, and have a good grip on what parts exactly
 * make the code not very efficient :)
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
//#define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */

/* What is the correct alignment? */
#define ALIGNMENT 16

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}


// Refrenced idea of doubly linked list online from geeksforgeeks.org
struct block{
    // Use either prev or next
    size_t size;
    struct block *prev;
    struct block *next;
};

// List of blocks
static struct block* flist = NULL;
//struct block *ulist;


/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
    /* Initialize memory */
    void *header =  mem_sbrk(align(sizeof(struct block)));
    if(header == (void *)(-1)){
       return false;
    }
    // Initialize free list to header
    flist = header;
    flist->next = NULL;
    flist->prev = NULL;
    flist->size = sizeof(struct block);
    return true;
}

//Add new allocted block to head of list
void AddBlock(struct block* newblock){
    struct block* oldlist = flist;
    newblock->prev = NULL;
    newblock->next =  oldlist;
    flist  = newblock;
    if(oldlist != NULL){
        oldlist->prev =  newblock;
    }
}

void RemoveBlock(struct block* removeblock){
    if(removeblock->prev == NULL){
        flist = removeblock->next;
    }
    else{
        removeblock->prev->next =  removeblock->next;
    }
    if(removeblock->next != NULL){
        removeblock->next->prev = removeblock->prev;
    }
}

/* support function that will search the list of free space until it finds one of the appropriate size, and 
and then splits it into 2 block structs, one being the size passed, and the other being the left over free space
*/
void split(struct block *open_slot, size_t size){
    // create and initialize leftover block
    struct block *leftover = (void *)open_slot+size+sizeof(struct block);
    // set leftover to appropriate size
    leftover->size = (open_slot->size)-size-sizeof(struct block);
    // assign the new block to the appropriate place in the list (in between open_slot and what was after open_slot)
    leftover->next = open_slot->next;
    // set the size of open_slot to the size we want
    open_slot->size = size;
    // finish connecting leftover to the doubly linked list
    leftover->prev = open_slot;
    // connect open_lot to new leftover block struct in LL
    open_slot->next = leftover;
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    /* IMPLEMENT THIS */
    if(size <= 0){
        return NULL;
    }
     
    // pointer to the beginning of the list *local* 
    struct block* curr = flist;
    void *pointer;
    size_t adjusted_size =  align(size);

    // CHECK IF THE SIZE IS TOO BIG FOR THE WHOLE HEAP TO PREVENT TRAVERSING THE LIST FOR NO REASON
    if(adjusted_size > mem_heapsize()){
        pointer = mem_sbrk(adjusted_size); 

        //CHECKS POINTER
        if(pointer == (void *)(-1)){
            return NULL;
        }
        return pointer;
    }    

    // TRAVERSES THE LIST FOR APPROPRIATELY SIZED BLOCKS
    while(curr->next != NULL){
        // IF THE CURRENT FREE BLOCK IS TOO SMALL, GO TO THE NEXT
        if(curr->size < adjusted_size){
            curr = curr->next;
        }
        // IF WE FOUND AN APPROPRIATELY SIZED POINTER, SET POINTER EQUAL TO IT AND RETURN IT
        else{
            split(curr, adjusted_size);
            pointer = curr;
            RemoveBlock(curr);
            return pointer;
        }
    }

    // THE WHILE LOOP ABOVE DIDNT CHECK THE LAST BLOCK IN THE FREE LIST, SO CHECK IT HERE
    if (curr->size >= adjusted_size){
        split(curr, adjusted_size);
        pointer = curr;
        RemoveBlock(curr);
        return pointer;
    }
    
    // ALL BLOCKS HAVE BEEN CHECKED AND ARE NOT OF APPROPRIATE SIZE, SO EXTEND THE HEAP APPROPRIATELY
    else{
        pointer = mem_sbrk(adjusted_size); 
        if(pointer == (void *)(-1)){
            return NULL;
        }
        return pointer;
    }
    return  pointer;
}


/*
 * free
 */
void free(void* ptr)
{
    // IF ITS NULL, DO NOTHING
    if(ptr == NULL){
        return;
    }

    // ADD THE BLOCK TO THE FLIST
    AddBlock(ptr);
    return;
}

/* OUR ERROR COULD BE ABOVE IN OUR FREE STATEMENT BECAUSE WE ARENT CHANGING THE ACTUAL INFORMAITON AT THAT BLOCK TO AN
"INITIALIZED" STATE, AND COULD RESULT IN SOME GARBLED BYTES INSIDE OF OUR REALLOC IF THIS SPACE IS WHERE MEMORY IS BEING
REALLOCATED */


/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    // realloc call with NULL ptr passed is identical to malloc(size)
    if(oldptr ==  NULL){
        return malloc(size);
    }
    // realloc call with 0 size is identical to calling free(oldptr)
    else if (size == 0)
    {
        free(oldptr);
        return oldptr;
    }
    // for all other cases
    else{
        // If size is bigger than the whole current heap, allocate more space and copy oldptr to there
        if(size > mem_heapsize()){
            void* newLocation = malloc(size);
            mem_memcpy(newLocation, oldptr, size);
            return newLocation;
        }
        // get the start of freelist
        struct block* curr = flist;

        size_t oldsize = strlen(oldptr);
        // if the size of the source is less than the size we are allocating it in, only memcpy oldSize, not full size
        // Traverse the list until we find a free block of appropriate size, and dont reach the end of the list
        while((((curr->size) < size+sizeof(struct block)) && (curr->next) != NULL)){
            curr = curr->next;
        }
        // If we reach the end of the list without an open appropriately sized free block, then expand the heap by size
        // and memcpy to the new location
        if( (curr->size < size+sizeof(struct block) ) ){
            void* newLocation = malloc(size);
            newLocation = mem_memcpy(newLocation, oldptr, size);
            free(oldptr);
            return newLocation;
        }

        else{
            // Block has the appropriate space (size of info plus the size of the new struct block) then split the free space,
            // copy into the first part of the split block and free the old pointer
            if((curr->size) > size+sizeof(struct block)){
                split(curr, size);
                // if the bytes we are copying is less than the bytes of space we were required to allocate, only copy what old contained
                // leave the size-oldsize bytes uninitialized as explained in the README
                if (oldsize < size){
                    mem_memcpy((void *)(curr), oldptr, oldsize);
                }
                // otherwise, just copy of size bytes
                else{
                    mem_memcpy((void *)(curr), oldptr, size);
                }
                free(oldptr);
                return (void *)(curr);
            }
            // if there wont be leftover space (aka its equal to size + sizeof stuct block)
            else{
                // if the bytes we are copying is less than the bytes of space we were required to allocate, only copy what old contained
                // leave the size-oldsize bytes uninitialized as explained in the README
                if (oldsize < size){
                    mem_memcpy((void *)(curr), oldptr, oldsize);
                }
                // otherwise, just copy of size bytes
                else{
                    mem_memcpy((void *)(curr), oldptr, size);
                }
                free(oldptr);
                return (void *)(curr);
            }
        }
    }
    return NULL;
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno)
{
#ifdef DEBUG
    /* Write code to check heap invariants here */
    /* IMPLEMENT THIS */
    //Check free list is all free
    struct block* start = flist;
    while(start->next != NULL){
        if(start->free == 0){
            dbg_printf(" Free list not all free");
            return false;
        }
        start = start->next;
    }

    // LAST MINUTE CHANGES CAUSED FOR 4 HEAP CHECKER FUNCTIONS TO BE REMOVED FROM FINAL COMMIT -- SEE PREVIOUS COMMITS FROM MATT BRANCH
    
#endif /* DEBUG */
    return true;
}