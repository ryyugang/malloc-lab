#include <stdio.h>

extern int mm_init (void);
static void *extend_heap(size_t words);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
static void *coalesce (void *bp);
extern void *mm_realloc(void *ptr, size_t size);
static void *find_fit(size_t size);
static void place(void *bp, size_t asize);

/* MY MACRO */
#define WSIZE 4 // word & header/footer size 4 bytes
#define DSIZE 8 // double word size 8 bytes
#define CHUNKSIZE (1<<12) // extend heap by this amout 

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc)) // pack a size and allocated bit into a word 
#define GET(p) (*(unsigned int *)(p)) // read a word at address p
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // write a word at address p
#define GET_SIZE(p) (GET(p) & ~0x7) // read the size from address p
#define GET_ALLOC(p) (GET(p) & 0x1) // allocated fields from address p
#define HDRP(bp) ((char *)(bp) - WSIZE) // compute address of its header
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // compute address of its footer
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // compute address of next block
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // compute address of previous block

// #define FIRST_FIT
#define NEXT_FIT
// #define BEST_FIT
/* 
 * Students work in teams of one or two.  Teams enter their team name, 
 * personal names and login IDs in a struct of this
 * type in their bits.c file.
 */
typedef struct {
    char *teamname; /* ID1+ID2 or ID1 */
    char *name1;    /* full name of first member */
    char *id1;      /* login ID of first member */
    char *name2;    /* full name of second member (if any) */
    char *id2;      /* login ID of second member */
} team_t;

extern team_t team;

