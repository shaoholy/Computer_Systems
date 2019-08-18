# assignment-3-fangsong
assignment-3-fangsong

## Team
Full Name: Boran Shao, Fang Song  
CCIS-ID: boranshao, fangsong




## Design Document
The whole memory has a dummy head in the front as the original K&R version did.

    Dummy head -> whole data


   Data is consist of blocks. Each block has 2 header: one in front and one in back. Between them is the payload area.  
    The first 2 spaces in the payload area allocate to 2 pointers: prev and next.   
    They point to the previous free block and the next free block respectively.  

    Header | (prev pointer) (next pointer) payload area | Header


   The header compose of a bit and a size.   
    The bit indicates whether this block has been allocated(1-allocated, 0-free),   
    and the size tells us the size of this block(including 2 headers).
    The total block size is always a multiple of two headers.



### First fit and Best fit
   There are 2 strategies for finding the free block in malloc function:   
    If using "first fit" strategy, we can find the block very fast, but may require more efforts and time to split block.   
    If using "best fit" strategy, we can avoid fragmentation, also spare time splitting blocks in some cases, but traverse time maybe longer. 



### Advantage and Disadvantage
 Compared with the other three implementation, this design have some **advantages**:
  1.	We can simply set the allocated flag to 0 to free the block, we don’t have to traverse the whole free list to find the insertion position of this block as K&R did. This saves some time for free function.
  2.	We can easily check whether this block to be freed is allocated or already been freed, whereas the K&R can’t realize such inspect.
  3.	With the front and back header of block, we can use cur[-1] to locate the previous block, and cur[size] to locate the next block. We can also use cur[1] to get the previous free block, and use cur[2] to get the next empty block. These features provide lots of convenience during the coalescing process. Other 3 implementations don’t provide such convenience.

This design also have some **disadvantages** that can be improved in future:
  1.	Compared with K&R, it need more space because of the two header and prev pointer.
  2.	The malloc function still need traverse the list to find suitable free block.


