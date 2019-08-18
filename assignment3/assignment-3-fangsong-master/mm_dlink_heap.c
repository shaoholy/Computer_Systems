/*
 * The whole memory has a dummy head in the front as the original K&R version did.
 *
 * Dummy head -> whole data
 *
 *
 * Data is consist of blocks.
 * Each block has 2 header: one in front and one in back.
 * Between them is the payload area. The first 2 spaces in the payload area allocate to 2 pointers: prev and next.
 * They point to the previous free block and the next free block respectively.
 *
 * Header | (prev pointer) (next pointer) payload area | Header
 *
 *
 * The header compose of a bit and a size.
 * The bit indicates whether this block has been allocated(1-allocated, 0-free),
 * and the size tells us the size of this block(including 2 headers).
 * The total block size is always a multiple of two headers.
 *
 */


#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include "memlib.h"
#include "mm_heap.h"

#define max(x,y) ( x>y?x:y ) //Math.max

/*
 * Header of allocated block
 */
typedef union Header {          /* block header/footer */
    struct {
        size_t is_alloc: 1;                 		// 1 : allocated; 0 : free
        size_t block_size: 8 * sizeof(size_t) - 1; // size of this block including header and footer

    } s;
    union Header *ptr;							// point to the next/previous free block
    max_align_t x;              				// force alignment to max align boundary
} Header;

static const size_t MIN_BLOCK_SIZE = 4;  // header + footer + prev + next (for free list)

// forward declarations
static void initialize(void);
static void add_freeblock(Header *cur);
static Header *find_allocateblock(void *ap);
static Header *morecore(size_t);
void visualize(const char*);

/** Start of free memory list */
static Header *dummy_head = NULL;

/**
 * Initialize memory allocator.
 */
void mm_init() {
	mem_init();
	initialize();

}

/**
 * Reset memory allocator
 */
void mm_reset() {
	mem_reset_brk();
	initialize();

}

/**
 * De-initialize memory allocator
 */
void mm_deinit() {
	mem_deinit();
	initialize();
}


/**
 * Allocation units for nbytes in units of header size
 *
 * @param nbytes number of bytes
 * @return number of units for nbytes
 */
inline static size_t mm_units(size_t nbytes) {
    /* smallest count of Header-sized memory chunks */
    return (nbytes + sizeof(Header) - 1) / sizeof(Header) + 2; ///+2 additional chunk for the Headers
}

/**
 * Allocation nbytes in units of header size
 *
 * @param nunits number of units
 * @return number of bytes for nunits
 */
inline static size_t mm_bytes(size_t nunits) {
    return nunits * sizeof(Header);
}


/**
 * Allocates size bytes of memory and returns a pointer to
 * allocated memory, or NULL if storage cannot be allocated.
 *
 * @param nbytes the number of bytes to allocate
 * @return pointer to allocated memory or NULL if not available
 */
void *mm_malloc(size_t nbytes) {
	//edge case
    if (dummy_head == NULL) {
    	mm_init();
    }

    size_t nunits = max(mm_units(nbytes), MIN_BLOCK_SIZE); //transfer to units

    //find first fit free block
	Header *p = dummy_head;

    /* traverse the list to find a block */
    for (Header *cur = p[2].ptr;;cur = cur[2].ptr) {
    	// find first fit
    	if ((cur[0].s.is_alloc == 0) && (cur[0].s.block_size >= nunits)) {	//big enough
            if (cur[0].s.block_size < nunits + MIN_BLOCK_SIZE) {  			//too small, cannot split
            	if (dummy_head == cur) { //move dummy head
            		dummy_head = cur[1].ptr;
            	}

            	Header *next = cur[2].ptr; // unlink allocated block from free list
            	Header *prev = cur[1].ptr;
            	prev[2].ptr = next;
                next[1].ptr = prev;
                cur[0].s.is_alloc = cur[cur[0].s.block_size - 1].s.is_alloc = 1;  // mark allocat
            } else {													  // split
            	//the left out part
            	size_t left_size = cur[0].s.block_size - nunits;
                cur[left_size - 1].s.block_size = cur[0].s.block_size = left_size;

                //the return part
                cur += left_size;
                cur[0].s.block_size = cur[nunits - 1].s.block_size = nunits;
                cur[0].s.is_alloc = cur[nunits - 1].s.is_alloc = 1;  //mark allocate
            }
            return cur + 1; //return payload
        }

    	// back where we started and nothing found
        // so we need to get more storage
        if (cur == dummy_head) {                    /* wrapped around free list */
        	cur = morecore(nunits);
        	if (cur == NULL) {
                return NULL;                /* none left */
            }
        }
    }
}


/**
 * Deallocates the memory allocation pointed to by ptr.
 * If ptr is NULL or points to memory already freed or storage that is not part of the heap, no operation is performed.
 * ptr can point to a location that is interior to the memory being freed.
 *
 * @param ptr the allocated storage to free
 */
void mm_free(void *ptr) {
	//edge case
    if (ptr == NULL) {
        return;
    }

	Header *p = find_allocateblock(ptr);
	if (p == NULL) { //points to memory already freed or storage that is not part of the heap
		return;
	}

	add_freeblock(p); //free this block
}


/**
 * Reallocates size bytes of memory and returns a pointer to the allocated memory
 * If ptr is NULL or points to memory already freed or storage that is not part of the heap, no operation is performed.
 * ptr can point to a location that is interior to the memory being freed.
 *
 * @param ptr the currently allocated storage
 * @param nbytes the number of bytes to allocate
 * @return pointer to allocated memory or NULL if not available.
 */
void *mm_realloc(void *ptr, size_t nbytes) {
	//edge case
	if (ptr == NULL) {
		return mm_malloc(nbytes);
	}

	Header *p = find_allocateblock(ptr);
	if (p == NULL) { //points to memory already freed or storage that is not part of the heap
		return NULL;
	}

    size_t nunits = mm_units(nbytes); //transfer to units
    if (p->s.block_size >= nunits) { //enough space
    	return ptr;
    }

    size_t oldsize = p->s.block_size; //save old size before malloc changes it

    // allocate new block for request
    Header *newp = mm_malloc(nbytes); // pointer to new payload
    if (newp == NULL) {
    	return NULL;
    }

    size_t bytes = mm_bytes(oldsize - 2); //payload bytes, exclude header and footer
    memcpy(newp, ptr, bytes); // copy current payload to new payload's place

    mm_free(ptr);  // free input storage

    return newp;
}




/**
 * Returns true if ap is a header-aligned block pointer.
 * Note that C provides no direct way to do this, so this
 * is an approximation that should be relatively portable.
 *
 * @param ap an allocated pointer
 * @return true if pointer is a header-aligned block pointer
 */
inline static bool is_block_pointer(void* ap) {
	// lower sizeof(max_align_t)-1 bits of address should be 0
    return ((uintptr_t)ap & (sizeof(max_align_t)-1)) == 0;
}

/**
 * Reset heap and free list
 */
static void initialize() {
    //edge case
	if (mem_sbrk((MIN_BLOCK_SIZE + 1) * sizeof(Header)) == NULL) {
		return;
	}

	//dummy head
	dummy_head = mem_heap_lo();
	dummy_head[0].s.block_size = dummy_head[MIN_BLOCK_SIZE-1].s.block_size = MIN_BLOCK_SIZE;
	dummy_head[0].s.is_alloc = dummy_head[MIN_BLOCK_SIZE-1].s.is_alloc = 1;
	dummy_head[1].ptr = dummy_head[2].ptr = dummy_head;

	//tail
//	Header *tail = dummy_head + MIN_BLOCK_SIZE;
//	tail[0].s.block_size = 1;
//	tail[0].s.is_alloc = 1;
}





/**
 * add block to free list, coalescing adjacent blocks if necessary
 *
 * @param cur the free blocks to be added
 */
static void add_freeblock(Header *cur) {
	size_t size = cur->s.block_size;

	if (cur[-1].s.is_alloc == 0) {  //join lower block
		size_t lower_size = cur[-1].s.block_size;
		cur -= lower_size;
		size += lower_size;
		cur[0].s.block_size = cur[size-1].s.block_size = size;
	} else  { 					  // add block after dummy_head
		Header *next = dummy_head[2].ptr;
		cur[1].ptr = dummy_head;
		cur[2].ptr = next;
		dummy_head[2].ptr = next[1].ptr = cur;
	}
	dummy_head = cur; //set dummy_head to freed block after coalescing


	if (cur[size].s.is_alloc == 0) {   // join upper block
		Header *p = cur + size;
		Header *next = p[2].ptr;
		Header *prev = p[1].ptr;
		prev[2].ptr = next;
	    next[1].ptr = prev;

	    size_t upper_size = cur[size].s.block_size;
	    size += upper_size;
		cur[0].s.block_size = cur[size-1].s.block_size = size;
	}
	cur[0].s.is_alloc = cur[size-1].s.is_alloc = 0; // mark free
}

/**
 * Find allocated block's header.
 *
 * @param ap pointer to allocated storage
 */
static Header *find_allocateblock(void *ap) {
	//edge case
    if (ap == NULL || ap <= mem_heap_lo() || ap >= mem_heap_hi()) {
        return NULL;
    }

    Header *cur;
    //cur is the header
    if (is_block_pointer(ap)) { //check is block pointer
		cur = (Header*)ap - 1;
		if (cur[0].s.is_alloc == 1) {  //check allocated
			size_t nunits = cur[0].s.block_size;
			if ((nunits >= MIN_BLOCK_SIZE) && ((cur[0].s.is_alloc == cur[nunits-1].s.is_alloc)|| (cur[0].s.block_size == cur[nunits-1].s.block_size))) {
				return cur;
			}
		}
    }

    // cur is in the middle
    cur = mem_heap_lo();
    Header *next = cur + cur[0].s.block_size;
    while (next <= ap) {
    	cur = next;
    	next += cur[0].s.block_size;
    }

    if (cur[0].s.is_alloc == 1) {
    	return cur;
    }
    return NULL;
}

/**
 * Request additional memory to be added to this process.
 *
 * @param nu the number of Header-chunks to be added
 */
static Header *morecore(size_t nu) {
	// nalloc based on page size
	size_t nalloc = mm_units(mem_pagesize()) - 2;

    /* get at least NALLOC Header-chunks from the OS */
    if (nu < nalloc) {
        nu = nalloc;
    }

    size_t nbytes = mm_bytes(nu); // number of bytes
    char *cp = (char *) mem_sbrk(nbytes);
    if (cp == (char *) -1) {                 /* no space at all */
        return NULL;
    }

    //head
    Header *cur = (Header*)cp - 1;
    cur[0].s.block_size = cur[nu-1].s.block_size = nu;
    cur[0].s.is_alloc = cur[nu-1].s.is_alloc = 0;

    //tail
	cur[nu].s.block_size = 1;
	cur[nu].s.is_alloc = 1;

	/* add the new space to free list */
    add_freeblock(cur);
    //mm_free(cur + 1);

    return dummy_head;
}

/**
 * Print the free list (educational purpose)
 *
 * @msg the initial message to print
 */
void visualize(const char* msg) {
    fprintf(stderr, "\n--- Free list after \"%s\":\n", msg);

    if (dummy_head == NULL) {                   /* does not exist */
        fprintf(stderr, "    List does not exist\n\n");
        return;
    }

    if (dummy_head == dummy_head[1].ptr) {          /* self-pointing list = empty */
        fprintf(stderr, "    List is empty\n\n");
        return;
    }

    Header *tmp = dummy_head;
    char *str = "    ";
    do {           /* traverse the list */
		fprintf(stderr, "0x%p: %s blocks: %zu alloc: %d prev: 0x%p next: 0x%p\n", tmp, str, tmp[0].s.block_size, tmp[0].s.is_alloc, tmp[1].ptr, tmp[2].ptr);
		str = " -> ";
		tmp = tmp[2].ptr;
    }  while (tmp != dummy_head);
    fprintf(stderr, "--- end\n\n");
}


/**
 * Calculate the total amount of available free memory
 * excluding headers and footers.
 *
 * @return the amount of free memory in bytes
 */
size_t mm_getfree(void) {
    if (dummy_head == NULL) {
        return 0;
    }

    Header *tmp = dummy_head;
    size_t res = tmp[0].s.block_size;

    while (tmp[1].ptr != dummy_head) {
    	if (tmp[0].s.is_alloc == 0) {  // not dummy node
			res += tmp[0].s.block_size - 2;  // not headers/footers
			tmp = tmp[1].ptr;
    	}
    }

    return mm_bytes(res);
}
