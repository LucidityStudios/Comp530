/* -*- mode:c; c-file-style:"k&r"; c-basic-offset: 4; tab-width:4; indent-tabs-mode:nil; mode:auto-fill; fill-column:78; -*- */
/* vim: set ts=4 sw=4 et tw=78 fo=cqt wm=0: */

/* Tar Heels Allocator
 *
 * Simple Hoard-style malloc/free implementation.
 * Not suitable for use for large allocatoins, or
 * in multi-threaded programs.
 *
 * to use:
 * $ export LD_PRELOAD=/path/to/th_alloc.so <your command>
 */

/* Hard-code some system parameters */

#define SUPER_BLOCK_SIZE 4096
#define SUPER_BLOCK_MASK (~(SUPER_BLOCK_SIZE-1))
#define MIN_ALLOC 32 /* Smallest real allocation.  Round smaller mallocs up */
#define MAX_ALLOC 2048 /* Fail if anything bigger is attempted.
                        * Challenge: handle big allocations */
#define RESERVE_SUPERBLOCK_THRESHOLD 2

#define FREE_POISON 0xab
#define ALLOC_POISON 0xcd

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#define assert(cond) if (!(cond)) __asm__ __volatile__ ("int $3")

/* Object: One return from malloc/input to free. */
struct __attribute__((packed)) object {
    union {
        struct object *next; // For free list (when not in use)
        char * raw; // Actual data
    };
};

/* Super block bookeeping; one per superblock.  "steal" the first
 * object to store this structure
 */
struct __attribute__((packed)) superblock_bookkeeping {
    struct superblock_bookkeeping * next; // next super block
    struct object *free_list;
    // Free count in this superblock
    uint8_t free_count; // Max objects per superblock is 128-1, so a byte is sufficient
    uint8_t level;
};

/* Superblock: a chunk of contiguous virtual memory.
 * Subdivide into allocations of same power-of-two size. */
struct __attribute__((packed)) superblock {
    struct superblock_bookkeeping bkeep;
    void *raw;  // Actual data here
};


/* The structure for one pool of superblocks.
 * One of these per power-of-two */
struct superblock_pool {
    struct superblock_bookkeeping *next;
    uint64_t free_objects; // Total number of free objects across all superblocks
    uint64_t whole_superblocks; // Superblocks with all entries free
};

// 10^5 -- 10^11 == 7 levels
#define LEVELS 7
static struct superblock_pool levels[LEVELS] = {{NULL, 0, 0},
                                                {NULL, 0, 0},
                                                {NULL, 0, 0},
                                                {NULL, 0, 0},
                                                {NULL, 0, 0},
                                                {NULL, 0, 0},
                                                {NULL, 0, 0}};

/* Data structure to track large objects (bigger than MAX_ALLOC.
 * We will just do a simple linked-list by default.
 *
 * Extra credit to do something more substantial (or before seeing this code).
 */
struct big_object {
    struct big_object *next;
    void *addr;
    size_t size;
};

// List of big objects
static struct big_object *big_object_list = NULL;

/* Convert the size to the correct power of two.
 * Recall that the 0th entry in levels is really 2^5,
 * the second level represents 2^6, etc.
 *
 * Return the index to the appropriate level (0..6), or
 * -1 if the size is too large.
 */

static inline int size2level (ssize_t size) {
   /* Your code here. */
    // Temporarily suppress the compiler warning that size is unused
    // You should remove the following line
	if(size <= 32)
		return 0;
	if(size <= 64)
		return 1;
	if(size <= 128)
		return 2;
	if(size <= 256)
		return 3;
	if(size <= 512)
		return 4;
	if(size <= 1024)
		return 5;
	if(size <= 2048)
		return 6;
	return -1;

}

/* This function allocates and initializes a new superblock.
 *
 * Note that a superblock in this lab is only one 4KiB page, not
 * 8 KiB, as in the hoard paper.
 *
 * This design sacrifices the first entry in every superblock
 * to store a superblock_bookkeeping structure.  Yes,
 * it is a bit wasteful, but let's keep the exercise simple.
 *
 * power: the power of two to store in this superblock.  Note that
 *        this is offset by 5; so a power zero means 2^5.
 *
 * Return value: a struct superblock_bookkeeping, which is
 * embedded at the start of the superblock.  Or NULL on failure.
 */
static inline
struct superblock_bookkeeping * alloc_super (int power) {
	//1<<3 = 1 * 2 ** 3
    void *page;
    struct superblock* sb;
    int free_objects = 0, bytes_per_object = 0;
    char *cursor;
    // Your code here
    // Allocate a page of anonymous memory
	page = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_FILE|MAP_PRIVATE, -1, 0);
    // WARNING: DO NOT use brk---use mmap, lest you face untold suffering
    sb = (struct superblock*) page;
    // Put this one the list.
    sb->bkeep.next = levels[power].next;
    levels[power].next = &sb->bkeep;
    levels[power].whole_superblocks++;
    sb->bkeep.level = power;
    sb->bkeep.free_list = NULL;

    // Your code here: Calculate and fill the number of free objects in this superblock
    //  Be sure to add this many objects to levels[power]->free_objects, reserving
    //  the first one for the bookkeeping.
    // Be sure to set free_objects and bytes_per_object to non-zero values.
//	free_objects = pow(2,12)/pow(2,power+5)-1;
	free_objects = (1<<12)/(1<<(power+5))-1;
//	bytes_per_object = pow(2, power+5);
	bytes_per_object = 1<<(power+5);
	levels[power].free_objects += free_objects;
	sb->bkeep.free_count = free_objects;
    // The following loop populates the free list with some atrocious
    // pointer math.  You should not need to change this, provided that you
    // correctly calculate free_objects.

    cursor = (char *) sb;
    // skip the first object
    for (cursor += bytes_per_object; free_objects--; cursor += bytes_per_object) {
        // Place the object on the free list
        struct object* tmp = (struct object *) cursor;
        tmp->next = sb->bkeep.free_list;
        sb->bkeep.free_list = tmp;
    }
    return &sb->bkeep;
}

void *malloc(size_t size) {
    struct superblock_pool *pool;
    struct superblock_bookkeeping *bkeep;
    void *rv = NULL;
    int power = size2level(size);

    // Handle bigger allocations with mmap, and a simple list
    if (size > MAX_ALLOC) {
        // Why, yes we can do a recursive malloc.  But carefully...
        struct big_object *biggun = malloc(sizeof(struct big_object));
        biggun->next = big_object_list;
        biggun->addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(biggun->addr);
        biggun->size = size;
        big_object_list = biggun;
        return biggun->addr;
    }

    // Delete the following two lines
 //   errno = -ENOMEM;
 //   return rv;

    pool = &levels[power];

    if (!pool->free_objects) {
        bkeep = alloc_super(power);
    } else {
        bkeep = pool->next;
    }

    for ( ; bkeep != NULL; bkeep = bkeep->next) { //looping through the superblocks
        if (bkeep->free_count) { //if this superblock has a free block we can use
            struct object *cursor = bkeep->free_list;
			
			/* Remove an object from the free list. */
            // Your code here
			//if(bkeep->free_count == pow(2,12)/pow(2,power+5)-1) //if all blocks are free
			if(bkeep->free_count == (1<<12)/(1<<(power+5))-1) //if all blocks are free
				levels[power].whole_superblocks--;
			levels[power].free_objects--;
			bkeep->free_count--;
			bkeep->free_list = bkeep->free_list->next; //update free_list pointer
            //
            // NB: If you take the first object out of a whole
            //     superblock, decrement levels[power]->whole_superblocks
            // Temporarily suppress the compiler warning that cursor is unused
            // You should remove the following line
            rv = (void *) cursor;
            break;
        }
    }

    // assert that rv doesn't end up being NULL at this point
    assert(rv != NULL);

	//memset(ptr, ALLOC_POISON, pow(2, power+5));
	memset(rv, ALLOC_POISON, 1<<(power+5));
    /* Exercise 3: Poison a newly allocated object to detect init errors.
     * Hint: use ALLOC_POISON
     */
    return rv;
}

static inline
struct superblock_bookkeeping * obj2bkeep (void *ptr) {
    uint64_t addr = (uint64_t) ptr;
    addr &= SUPER_BLOCK_MASK;
    return (struct superblock_bookkeeping *) addr;
}

void free(void *ptr) {

    // Just ignore free of a null ptr
    if (ptr == NULL) return;
    
    struct superblock_bookkeeping *bkeep = obj2bkeep(ptr);
    int power = bkeep->level;

    // We need to check for free of any large objects first.
    {
        struct big_object *tmp, *last;
        for(tmp = big_object_list, last = NULL; tmp; last = tmp, tmp = tmp->next) {
            if (tmp->addr == ptr) {
                // We found it
                // Unmap the object
                munmap(tmp->addr, tmp->size);
                // Fix up the list
                if (!last) {
                    big_object_list = tmp->next;
                } else {
                    last->next = tmp->next;
                }
                // Free the node
                free(tmp);
                return;
            }
        }
    } 

    /* Exercise 3: Poison a newly freed object to detect use-after-free errors.
     * Hint: use FREE_POISON.
     */
	//memset(ptr, FREE_POISON, pow(2, power+5));
	memset(ptr, FREE_POISON, 1<<(power+5));
   
   // Your code here.
    //   Be sure to put this back on the free list, and update the
    //   free count.  If you add the final object back to a superblock,
	struct object *obj_ptr = (struct object*) ptr;
	obj_ptr->next = bkeep->free_list;
	bkeep->free_list = obj_ptr;
	
	bkeep->free_count++;
	levels[power].free_objects++;

//	if(bkeep->free_count == pow(2,12)/pow(2,(bkeep->level)+5)-1) //if all blocks are free
	if(bkeep->free_count == (1<<12)/(1<<(power+5))-1) //if all blocks are free
		levels[bkeep->level].whole_superblocks++;
    //   making all objects free, increment whole_superblocks.

	struct superblock_bookkeeping *wholestart = levels[power].next;
	struct superblock_bookkeeping *old_temp = NULL;
    while (levels[power].whole_superblocks > RESERVE_SUPERBLOCK_THRESHOLD) {
        // Exercise 4: Your code here
        // Remove a whole superblock from the level
        // Return that superblock to the OS, using mmunmap
//		if(wholestart->free_count == pow(2,12)/pow(2,power+5)-1) { //if this superblock is whole
		if(wholestart->free_count == 4096/(1<<(power+5))-1) { //if this superblock is whole
			if(old_temp == NULL) {
				wholestart = wholestart->next;
				//unmap levels[power].next
				munmap(levels[power].next, 4096);
				levels[power].whole_superblocks--;
				levels[power].free_objects -= (1<<12)/(1<<(power+5));	
				//unmapped levels[power].next
				levels[power].next = wholestart;
			}
			else {
				wholestart = wholestart->next;
				//unmap old_temp->next
				munmap(old_temp->next, 4096);
				levels[power].whole_superblocks--;
				levels[power].free_objects -= (1<<12)/(1<<(power+5));
				//unmapped old_temp->next
				old_temp->next = wholestart;
			}
		}
		else {
			if(old_temp == NULL)
				old_temp = levels[power].next;
			else
				old_temp = old_temp->next;
			wholestart = wholestart->next;
		}
		//loop through levels[power].next until find one that has
        //break; // hack to keep this loop from hanging; remove in ex 4
    }

}

// Do NOT touch this - this will catch any attempt to load this into a multi-threaded app
int pthread_create(void __attribute__((unused)) *x, ...) {
    exit(-ENOSYS);
}
