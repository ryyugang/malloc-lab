/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"
#define First_fit
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

void *heap_listp; // 힙의 초기 영역을 가리키는 포인터
void *lastp; // next_fit 방식에 사용하는 포인터

#ifdef Segmented_free_list
void *SFL[SFLsize + 1]; // Segmented Free list 선언
#endif

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   
#ifdef Segmented_free_list
    // Segmented Free list 초기화
    for (int list = 0 ; list <= SFLsize; list ++)
    {
        SFL[list] = NULL;
    }
#endif    

    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *) -1)
    {
        return -1;
    }
    
    PUT(heap_listp, 0); // 힙 구분용 첫 블록
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // 프롤로그 블록 2개중 Header block
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // 프롤로그 블록 2개중 Footer block
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); // 힙 구분용 마지막 블록
    heap_listp += (2*WSIZE); // 포인터를 2칸 움직이게 해서 Header block 다음에 위치시킴

    lastp = heap_listp; // next_fit 방식 사용 시 포인터를 항상 동기화 시켜야 함

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) // 더 이상 영역 확장이 불가능하면
    {
        return -1; // 종료
    }
    return 0; // 리턴 값 없는 초기 함수
}

/* 
* extend_heap - Allocate an even number of words to maintain alignment
*               Initialize free block header/footer and the epilogue header
*               Coalesce if he previous block was free
*/
static void *extend_heap(size_t words) // 최대 힙 영역을 넘어가지 않는 안에서, 영역 확장
{
    char *bp;
    size_t size;

    // 블록 사이즈가 짝수면 그대로, 홀수면 패딩을 위해 1 워드 추가
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; 
    if ((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0)); // 현재 블록의 Header block 위치에 [사이즈, 0(FREE)] 입력
    PUT(FTRP(bp), PACK(size, 0)); // 현재 블록의 Footer block 위치에 [사이즈, 0(FREE)] 입력
    // 에필로그 블록 헤더 설정
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 다음 블록의 Header block 위치에 [0, 1(allocated)] 입력

    return coalesce(bp); // Free block 병합하는 함수 호출
                         // Free block이 새로 생성되었으니까 
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
#ifndef Segmented_free_list
    size_t asize; // 할당을 위해 조정할 사이즈
    size_t extendsize; 
    char *bp;

    if (size == 0) // 할당하려는 크기가 0이면 
    {
        return NULL;
    }
    
    if (size <= DSIZE) // 할당하려는 크기가 8 bytes보다 적다면 (Header, Footer block)
    {
        asize = 2 * DSIZE; // 조정할 사이즈를 4 word (최소 블록 사이즈, 1 + 2 + 1)로 설정
    }
    else // 할당하려는 크기가 8 bytes보다 크다면
    {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); // Padding 고려하여 사이즈 설정
    }

    if ((bp = find_fit(asize)) != NULL) // 할당하려는 사이즈를 할당할 위치를 찾았다면
    {
        place(bp, asize); // 할당
        lastp = bp; // next_fit 사용을 위해 포인터 동기화
        return bp;
    }

    // 할당하려는 사이즈를 할당할 위치를 찾지 못했다면
    extendsize = MAX(asize, CHUNKSIZE); // CHUNKSIZE는 최대 힙 크기
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) // 더 이상 확장이 불가능하면
    {
        return NULL; 
    }
    // 힙 확장 이후
    place(bp, asize); // 할당 
    lastp = bp; // 포인터 동기화
    return bp;
#endif

#ifdef Segmented_free_list
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
    {
        return NULL;
    }

    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    {
        return NULL;
    }

    place(bp, asize);
    return bp;
#endif
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) // 할당 해제
{
    size_t size = GET_SIZE(HDRP(ptr)); // 현재 블록 사이즈

    PUT(HDRP(ptr), PACK(size, 0)); // 현재 블록의 Header block에, Free block의 상태로 기입
    PUT(FTRP(ptr), PACK(size, 0)); // 현재 블록의 Footer block에, Free block의 상태로 기입
    coalesce(ptr); // 현재 block은 Free block이 되었으니, 병합함수 호출
}

/*
* ecoalesce - merge two or more free block
*/
static void *coalesce (void *bp) // 분할되어있는 Free block을 병합하여 1개의 block으로 취급하기 위해
{
    // 코드 작성 편리를 위해 변수 설정
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));  // 이전 블록의 할당여부
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록의 할당여부
    size_t size = GET_SIZE(HDRP(bp)); // 현재 블록 사이즈

#ifndef Segmented_free_list
    if (prev_alloc && next_alloc) // case1 : 이전, 다음 블록 allocated
    {
        return bp; // 병합이 불가능하니, 현재 포인터만 반환
    }

    else if (prev_alloc && !next_alloc) // case2 : 이전 블록 allocated, 다음 블록 free
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블록의 사이즈만큼 더해줌
        PUT(HDRP(bp), PACK(size, 0)); // 현재 블록의 Header block에, Free block의 상태로 기입
        PUT(FTRP(bp), PACK(size, 0)); // 현재 블록의 Footer block에, Free block의 상태로 기입
    }

    else if (!prev_alloc && next_alloc) // case3 : 이전 블록 free, 다음 블록 allocated
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); // 이전 블록 사이즈만큼 더해줌
        PUT(FTRP(bp), PACK(size, 0)); // 현재 블록의 Footer block에, Free block의 상태로 기입
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록의 Header block에, Free block의 상태로 기입
        bp = PREV_BLKP(bp); // 현재 블록을 가리키는 포인터를, 이전 블록을 가리키는 포인터로 업데이트
    }

    else // case4 : 이전 블록 free, 다음 블록 free
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 이전 + 다음 블록 사이즈만큼 더해줌
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 현재 블록의 Header block에, Free block의 상태로 기입
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // 다음 블록의 Footer block에, Free block의 상태로 기입
        bp = PREV_BLKP(bp); // 현재 블록을 가리키는 포인터를, 이전 블록을 가리키는 포인터로 업데이트
    }
    
    lastp = bp; // next_fit 사용을 위한 포인터 동기화
    return bp;
#endif

#ifdef Segmented_free_list
    if (prev_alloc && next_alloc) // case1 : 이전, 다음 블록 allocated
    {
        insert_SFL(bp);
        return bp; // 병합이 불가능하니, 현재 포인터만 반환
    }

    else if (prev_alloc && !next_alloc) // case2 : 이전 블록 allocated, 다음 블록 free
    {
        delete_SFL(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블록의 사이즈만큼 더해줌
        PUT(HDRP(bp), PACK(size, 0)); // 현재 블록의 Header block에, Free block의 상태로 기입
        PUT(FTRP(bp), PACK(size, 0)); // 현재 블록의 Footer block에, Free block의 상태로 기입
    }

    else if (!prev_alloc && next_alloc) // case3 : 이전 블록 free, 다음 블록 allocated
    {
        delete_SFL(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); // 이전 블록 사이즈만큼 더해줌
        PUT(FTRP(bp), PACK(size, 0)); // 현재 블록의 Footer block에, Free block의 상태로 기입
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록의 Header block에, Free block의 상태로 기입
        bp = PREV_BLKP(bp); // 현재 블록을 가리키는 포인터를, 이전 블록을 가리키는 포인터로 업데이트
    }

    else // case4 : 이전 블록 free, 다음 블록 free
    {
        delete_SFL(NEXT_BLKP(bp));
        delete_SFL(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 이전 + 다음 블록 사이즈만큼 더해줌
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 현재 블록의 Header block에, Free block의 상태로 기입
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // 다음 블록의 Footer block에, Free block의 상태로 기입
        bp = PREV_BLKP(bp); // 현재 블록을 가리키는 포인터를, 이전 블록을 가리키는 포인터로 업데이트
    }
    insert_SFL(bp);
    return bp;
#endif
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) // 재할당
{
#ifndef Segmented_free_list
    if (ptr == NULL) // 입력 포인터가 NULL이면, 입력 사이즈만큼 새롭게 할당 (예외처리)
    {
        return mm_malloc(size);
    }
    
    if (size == 0) // 입력 사이즈가 0이면, 입력 포인터의 블록을 해제 (예외처리)
    {
        mm_free(ptr);
        return NULL;
    }

    void *oldptr = ptr;
    size_t copySize = GET_SIZE(HDRP(oldptr)); // 재할당하려는 블록의 사이즈
    
    if (size + DSIZE <= copySize) // (재할당 하려는 블록 사이즈 + 8 bytes(Header + Footer)) <= 현재 블록 사이즈
    {
        return oldptr; // 현재 블록에 재할당해도 문제 없기 때문에, 포인터만 반환
    }
    else // (재할당 하려는 블록 사이즈 + 8 bytes) > 현재 블록 사이즈
         // 경우에 따라서 인접 Free block을 활용하는 방안과, 새롭게 할당하는 방안을 이용해야 함
    {
        size_t next_size = copySize + GET_SIZE(HDRP(NEXT_BLKP(oldptr))); // 현재 블록 사이즈 + 다음 블록 사이즈 = next_size
        size_t prev_size = copySize + GET_SIZE(HDRP(PREV_BLKP(oldptr))); // 이전 블록 사이즈

        if (!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && (size + DSIZE <= next_size)) 
        // 다음 블록이 Free block이고, (재할당 하려는 블록의 사이즈 + 8 bytes) <= (현재 블록 사이즈 + 다음 블록 사이즈)
        // 현재 블록과 다음 블록을 하나의 블록으로 취급해도 크기의 문제가 발생하지 않음
        // malloc을 하지 않아도 됨 -> 메모리 공간 및 시간적 이득을 얻을 수 있음
        {
            PUT(HDRP(oldptr), PACK(next_size, 1)); // 현재 블록의 Header Block에, (현재 블록 사이즈 + 다음 블록 사이즈) 크기와 Allocated 상태 기입
            PUT(FTRP(oldptr), PACK(next_size, 1)); // 현재 블록의 Footer Block에, (현재 블록 사이즈 + 다음 블록 사이즈) 크기와 Allocated 상태 기입
            lastp = oldptr; // next_fit 사용을 위한 포인터 동기화
            return oldptr;
        }
        else if (!GET_ALLOC(HDRP(PREV_BLKP(oldptr))) && (size + DSIZE <= prev_size))
        // 이전 블록이 Free block이고, (재할당 하려는 블록의 사이즈 + 8 bytes) <= (이전 블록 사이즈 + 현재 블록 사이즈)
        // 이전 블록과 현재 블록을 하나의 블록으로 취급해도 크기의 문제가 발생하지 않음
        // malloc을 하지 않아도 됨 -> 메모리 공간 및 시간적 이득을 얻을 수 있음
        {
            void *prev_ptr = PREV_BLKP(oldptr); // 이전 블록의 bp
            
            memmove(prev_ptr, oldptr, copySize); // 이전 블록의 bp로 현재 block의 메모리 영역을 옮긴다
            PUT(HDRP(prev_ptr), PACK(prev_size, 1)); // 이전 블록의 Header Block에, (이전 블록 사이즈 + 현재 블록 사이즈) 크기와 Allocated 상태 기입
            PUT(FTRP(prev_ptr), PACK(prev_size, 1)); // 이전 블록의 Footer Block에, (이전 블록 사이즈 + 현재 블록 사이즈) 크기와 Allocated 상태 기입
            lastp = prev_ptr; // next_fit 사용을 위한 포인터 동기화
            return prev_ptr;
        }
        else if (!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && !GET_ALLOC(HDRP(PREV_BLKP(oldptr))) && (size + DSIZE <= next_size + copySize + prev_size))
        // 이전 블록과 다음 블록이 모두 Free block, (재할당 하려는 블록의 사이즈 + 8 bytes) <= (이전 블록 사이즈 + 현재 블록 사이즈 + 다음 블록 사이즈)
        // 이전 블록과 현재 블록과 다음 블록을 하나의 블록으로 취급해도 크기의 문제가 발생하지 않음
        // malloc을 하지 않아도 됨 -> 메모리 공간 및 시간적 이득을 어등ㄹ 수 있음
        {
            void *prev_ptr = PREV_BLKP(oldptr); // 이전 블록의 bp

            memmove(prev_ptr, oldptr, copySize); // 이전 블록의 bp로 현재 block의 메모리 영역을 옮긴다
            PUT(HDRP(prev_ptr), PACK(prev_size + copySize + next_size, 1)); // 이전 블록의 Header Block에, (이전 블록 사이즈 + 현재 블록 사이즈 + 다음 블록 사이즈) 크기와 Allocated 상태 기입
            PUT(FTRP(prev_ptr), PACK(prev_size + copySize + next_size, 1)); // 이전 블록의 Footer Block에, (이전 블록 사이즈 + 현재 블록 사이즈 + 다음 블록 사이즈) 크기와 Allocated 상태 기입
            lastp = prev_ptr; // next_fit 사용을 위한 포인터 동기화
            return prev_ptr;
        }
                
        else // 위 케이스에 모두 해당되지 않아, 결국 malloc을 해야 하는 경우
        {
            void *newptr = mm_malloc(size + DSIZE); // (할당하려는 크기 + 8 bytes)만큼 새롭게 할당
            if (newptr == NULL) // 새로 할당한 주소가 NULL일 경우 (예외처리)
            {
                return NULL;
            }
            memmove(newptr, oldptr, size + DSIZE); // payload 복사
            lastp = newptr; // next_fit 사용을 위한 포인터 동기화
            mm_free(oldptr); // 기존의 블록은 Free block으로 바꾼다
            return newptr; // 새롭게 할당된 주소의 포인터를 반환
        }
    }
#endif

#ifdef Segmented_free_list
    if (ptr == NULL) // 입력 포인터가 NULL이면, 입력 사이즈만큼 새롭게 할당 (예외처리)
    {
        return mm_malloc(size);
    }
    
    if (size == 0) // 입력 사이즈가 0이면, 입력 포인터의 블록을 해제 (예외처리)
    {
        mm_free(ptr);
        return NULL;
    }

    void *oldptr = ptr;
    size_t copySize = GET_SIZE(HDRP(oldptr)); // 재할당하려는 블록의 사이즈
    
    if (size + DSIZE <= copySize) // (재할당 하려는 블록 사이즈 + 8 bytes(Header + Footer)) <= 현재 블록 사이즈
    {
        return oldptr; // 현재 블록에 재할당해도 문제 없기 때문에, 포인터만 반환
    }
    else // (재할당 하려는 블록 사이즈 + 8 bytes) > 현재 블록 사이즈
         // 경우에 따라서 인접 Free block을 활용하는 방안과, 새롭게 할당하는 방안을 이용해야 함
    {
        size_t next_size = copySize + GET_SIZE(HDRP(NEXT_BLKP(oldptr))); // 현재 블록 사이즈 + 다음 블록 사이즈 = next_size
        // size_t prev_size = copySize + GET_SIZE(HDRP(PREV_BLKP(oldptr))); // 이전 블록 사이즈

        if (!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && (size + DSIZE <= next_size)) 
        // 다음 블록이 Free block이고, (재할당 하려는 블록의 사이즈 + 8 bytes) <= (현재 블록 사이즈 + 다음 블록 사이즈)
        // 현재 블록과 다음 블록을 하나의 블록으로 취급해도 크기의 문제가 발생하지 않음
        // malloc을 하지 않아도 됨 -> 메모리 공간 및 시간적 이득을 얻을 수 있음
        {
            delete_SFL(NEXT_BLKP(oldptr));
            PUT(HDRP(oldptr), PACK(next_size, 1)); // 현재 블록의 Header Block에, (현재 블록 사이즈 + 다음 블록 사이즈) 크기와 Allocated 상태 기입
            PUT(FTRP(oldptr), PACK(next_size, 1)); // 현재 블록의 Footer Block에, (현재 블록 사이즈 + 다음 블록 사이즈) 크기와 Allocated 상태 기입
            return oldptr;  
        }
        // else if (!GET_ALLOC(HDRP(PREV_BLKP(oldptr))) && (size + DSIZE <= prev_size))
        // // 이전 블록이 Free block이고, (재할당 하려는 블록의 사이즈 + 8 bytes) <= (이전 블록 사이즈 + 현재 블록 사이즈)
        // // 이전 블록과 현재 블록을 하나의 블록으로 취급해도 크기의 문제가 발생하지 않음
        // // malloc을 하지 않아도 됨 -> 메모리 공간 및 시간적 이득을 얻을 수 있음
        // {
        //     void *prev_ptr = PREV_BLKP(oldptr); // 이전 블록의 bp
            
        //     memmove(prev_ptr, oldptr, copySize); // 이전 블록의 bp로 현재 block의 메모리 영역을 옮긴다
        //     delete_SFL(oldptr);
        //     PUT(HDRP(prev_ptr), PACK(prev_size, 1)); // 이전 블록의 Header Block에, (이전 블록 사이즈 + 현재 블록 사이즈) 크기와 Allocated 상태 기입
        //     PUT(FTRP(prev_ptr), PACK(prev_size, 1)); // 이전 블록의 Footer Block에, (이전 블록 사이즈 + 현재 블록 사이즈) 크기와 Allocated 상태 기입
        //     return prev_ptr;
        // }
        // else if (!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && !GET_ALLOC(HDRP(PREV_BLKP(oldptr))) && (size + DSIZE <= next_size + copySize + prev_size))
        // // 이전 블록과 다음 블록이 모두 Free block, (재할당 하려는 블록의 사이즈 + 8 bytes) <= (이전 블록 사이즈 + 현재 블록 사이즈 + 다음 블록 사이즈)
        // // 이전 블록과 현재 블록과 다음 블록을 하나의 블록으로 취급해도 크기의 문제가 발생하지 않음
        // // malloc을 하지 않아도 됨 -> 메모리 공간 및 시간적 이득을 어등ㄹ 수 있음
        // {
        //     void *prev_ptr = PREV_BLKP(oldptr); // 이전 블록의 bp

        //     memmove(prev_ptr, oldptr, copySize); // 이전 블록의 bp로 현재 block의 메모리 영역을 옮긴다
        //     delete_SFL(oldptr);
        //     delete_SFL(NEXT_BLKP(oldptr));
        //     PUT(HDRP(prev_ptr), PACK(prev_size + copySize + next_size, 1)); // 이전 블록의 Header Block에, (이전 블록 사이즈 + 현재 블록 사이즈 + 다음 블록 사이즈) 크기와 Allocated 상태 기입
        //     PUT(FTRP(prev_ptr), PACK(prev_size + copySize + next_size, 1)); // 이전 블록의 Footer Block에, (이전 블록 사이즈 + 현재 블록 사이즈 + 다음 블록 사이즈) 크기와 Allocated 상태 기입
        //     return prev_ptr;
        // }
                
        else // 위 케이스에 모두 해당되지 않아, 결국 malloc을 해야 하는 경우
        {
            void *newptr = mm_malloc(size + DSIZE); // (할당하려는 크기 + 8 bytes)만큼 새롭게 할당
            if (newptr == NULL) // 새로 할당한 주소가 NULL일 경우 (예외처리)
            {
                return NULL;
            }
            memmove(newptr, oldptr, size + DSIZE); // payload 복사
            mm_free(oldptr); // 기존의 블록은 Free block으로 바꾼다
            return newptr; // 새롭게 할당된 주소의 포인터를 반환
        }
    }
#endif
}


/*
* place
*/
static void place(void *bp, size_t asize) // 배치 및 분할
{
#ifndef Segmented_free_list    
    size_t csize = GET_SIZE(HDRP(bp)); // 입력 포인터가 위치한 블록 사이즈 설정

    if ((csize - asize) >= (2 * DSIZE)) // (블록 사이즈 - 조정할 사이즈)가 16 bytes보다 같거나 클 경우
                                        // 할당 하고도 공간이 남으니, 분할 작업이 이루어진다
    {
        PUT(HDRP(bp), PACK(asize, 1)); // 입력 포인터가 위치한 블록의 Header block에 [조정할 사이즈, Allocated] 상태 기입
        PUT(FTRP(bp), PACK(asize, 1)); // 입력 포인터가 위치한 블록의 Footer block에 [조정할 사이즈, Allocated] 상태 기입
        bp = NEXT_BLKP(bp); // 입력 포인터가 위치한 블록의 다음 블록으로 포인터 변경
        PUT(HDRP(bp), PACK(csize - asize, 0)); // 입력 포인터가 위치한 블록의 Header block에 [블록 사이즈 - 조정할 사이즈, Free] 상태 기입
        PUT(FTRP(bp), PACK(csize - asize, 0)); // 입력 포인터가 위치한 블록의 Footer block에 [블록 사이즈 - 조정할 사이즈, Free] 상태 기입
    }
    else // (블록 사이즈 - 조정할 사이즈)가 16 bytes보다 작을 경우
         // 분할작업 X
    {
        PUT(HDRP(bp), PACK(csize, 1)); // 입력 포인터가 위치한 블록의 Header block에 [블록 사이즈 - 조정할 사이즈, Allocated] 상태 기입
        PUT(FTRP(bp), PACK(csize, 1)); // 입력 포인터가 위치한 블록의 Footer block에 [블록 사이즈 - 조정할 사이즈, Allocated] 상태 기입
    }
#endif

#ifdef Segmented_free_list
    size_t csize = GET_SIZE(HDRP(bp)); // 입력 포인터가 위치한 블록 사이즈 설정
    delete_SFL(bp);

    if ((csize - asize) >= (2 * DSIZE)) // (블록 사이즈 - 조정할 사이즈)가 16 bytes보다 같거나 클 경우
                                        // 할당 하고도 공간이 남으니, 분할 작업이 이루어진다
    {
        PUT(HDRP(bp), PACK(asize, 1)); // 입력 포인터가 위치한 블록의 Header block에 [조정할 사이즈, Allocated] 상태 기입
        PUT(FTRP(bp), PACK(asize, 1)); // 입력 포인터가 위치한 블록의 Footer block에 [조정할 사이즈, Allocated] 상태 기입
        bp = NEXT_BLKP(bp); // 입력 포인터가 위치한 블록의 다음 블록으로 포인터 변경
        PUT(HDRP(bp), PACK(csize - asize, 0)); // 입력 포인터가 위치한 블록의 Header block에 [블록 사이즈 - 조정할 사이즈, Free] 상태 기입
        PUT(FTRP(bp), PACK(csize - asize, 0)); // 입력 포인터가 위치한 블록의 Footer block에 [블록 사이즈 - 조정할 사이즈, Free] 상태 기입
        coalesce(bp);
    }
    else // (블록 사이즈 - 조정할 사이즈)가 16 bytes보다 작을 경우
         // 분할작업 X
    {
        PUT(HDRP(bp), PACK(csize, 1)); // 입력 포인터가 위치한 블록의 Header block에 [블록 사이즈 - 조정할 사이즈, Allocated] 상태 기입
        PUT(FTRP(bp), PACK(csize, 1)); // 입력 포인터가 위치한 블록의 Footer block에 [블록 사이즈 - 조정할 사이즈, Allocated] 상태 기입
    }
#endif
}

/* 
* find_fit - first fit
* 요청이 들어오면, 처음부터 계속해서 탐색
*/
// #if_def 사용으로, FIT 방식을 define하여 선택
#ifdef FIRST_FIT_IMPLICIT 
// 힙의 첫 주소부터 탐색하며 할당 가능한 블록을 찾는 방식
static void *find_fit(size_t asize)
{
    void *bp;

    // 새롭게 정의한 포인터 bp를 힙 초기 영역을 가리키는 포인터로 설정
    // bp를 블록 단위로 이동시킴
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        // 블록이 Free block이고, 해당 블록의 사이즈가 입력 사이즈보다 크거나 같을 경우
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            // 해당 블록은 할당이 가능하므로, 해당 블록에 위치한 bp를 그대로 반환한다
            return bp;
        }
    }
    return NULL;
}
#endif

/* 
* find_fit - next fit
* 요청이 들어오면, 이전에 탐색했던 블록 이후부터 탐색
*/
#ifdef NEXT_FIT_IMPLICIT
// 최근에 할당이 이루어진 위치 이후로 탐색
static void *find_fit(size_t asize)
{
    void *bp;

    // 새롭게 정의한 포인터 bp를 최근 할당이 이루어진 위치의 포인터로 설정
    // bp를 블록 단위로 이동시킴
    for (bp = lastp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        // 블록이 Free block이고, 해당 블록 사이즈가 입력 사이즈보다 크거나 같을 경우
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            // 해당 블록은 할당이 가능하므로, 해당 블록에 위치한 bp를 반환하며 lastp 포인터를 동기화한다 
            lastp = bp;
            return bp;
        }
    }
    return NULL;
}
#endif


/*
* find_fit - best fit
*/
#ifdef BEST_FIT_IMPLICIT
static void *find_fit(size_t asize)
{
    void *bp;
    void *bestp = NULL;
    
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            if (bestp == NULL || GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(bestp)))
            {
                bestp = bp;
            }
        }
    }

    return bestp;
}
#endif

#ifdef FIRST_FIT_SFL
static void *find_fit(size_t asize)
{
    int index = find_index(asize);

    for (int i = index; i <= SFLsize; i++)
    {
        for (void *bp = SFL[i]; bp != NULL; bp = NEXT_FREE(bp))
        {
            if (asize <= GET_SIZE(HDRP(bp)))
            {
                return bp;
            }
        }
    }
    return NULL;
}
#endif

#ifdef Segmented_free_list
/*
* insert_SFL - SFL의 앞부터 Free block 추가 (LIFO)
*/
static void insert_SFL(void* bp)
{
    int index = find_index(GET_SIZE(HDRP(bp)));

    if (SFL[index] == NULL)
    {
        PREV_FREE(bp) = NULL;
        NEXT_FREE(bp) = NULL;
    }
    else
    {
        PREV_FREE(bp) = NULL;
        NEXT_FREE(bp) = SFL[index];
        PREV_FREE(SFL[index]) = bp;
    }
    SFL[index] = bp;
}

/*
* find_index - SFL 내에 적합한 사이즈 검색
*/
static int find_index(size_t size)
{
    int index = 0;

    while ((index < SFLsize - 1) && (size > 1))
    {
        size >>= 1;
        index++;
    }
    return index;
}

/*
* delete_SFL - SFL 내부에 존재하던 Free block을 특정 이유로 인해 제거
*/
static void delete_SFL(void *bp)
{
    int index = find_index(GET_SIZE(HDRP(bp)));

    if (SFL[index] == bp)
    {
        if (NEXT_FREE(bp) == NULL)
        {
            SFL[index] = NULL;
        }
        else
        {
            PREV_FREE(NEXT_FREE(bp)) = NULL;
            SFL[index] = NEXT_FREE(bp);
        }
    }
    else
    {
        if (NEXT_FREE(bp) != NULL)
        {
            PREV_FREE(NEXT_FREE(bp)) = PREV_FREE(bp);
            NEXT_FREE(PREV_FREE(bp)) = NEXT_FREE(bp);
        }
        else
        {
            NEXT_FREE(PREV_FREE(bp)) = NULL;
        }
    }
}
#endif