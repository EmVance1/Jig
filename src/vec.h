#ifndef CJIG_IMPL_VEC_H
#define CJIG_IMPL_VEC_H
#include "jig/jig.h"


typedef struct jigDynArrayHeader {
    size_t len;
    size_t cap;
} jigDynArrayHeader;

#ifndef JIGDS_INIT_LEN
#define JIGDS_INIT_LEN 8
#endif

#ifndef JIGDS_ALLOCATOR
#define JIGDS_ALLOCATOR (jig_malloc_allocator())
#endif


void* jigds_arrgrowf(void* ptr, size_t elemsize, size_t addlen, size_t min_cap, jigAllocator alloc);


#define jigds_arrgrow(a,b)        ((a) = jigds_arrgrowf((a), sizeof *(a), (b), JIGDS_INIT_LEN, JIGDS_ALLOCATOR))

#define jigds_arrmaybegrow(a,n)   ((!(a) || jigds_header(a)->len + (n) > jigds_header(a)->cap) ? (jigds_arrgrow(a,n),0) : 0)

#define jigds_header(arr)         ((jigDynArrayHeader*)(arr) - 1)
#define jigds_arraddn(arr, n)     (jigds_arrmaybegrow(arr,n), (n) ? (jigds_header(arr)->len += (n)) : (0))
#define jigds_arraddnptr(arr, n)  (jigds_arrmaybegrow(arr,n), (n) ? (jigds_header(arr)->len += (n), &(arr)[jigds_header(arr)->len-(n)]) : (arr))
#define jigds_arrpush(arr, val)   (jigds_arrmaybegrow(arr,1), (arr)[jigds_header(arr)->len++] = (val))
#define jigds_arrlenu(arr)        ((arr) ? jigds_header(arr)->len : 0)
#define jigds_arrlast(arr)        ((arr)[jigds_header(arr)->len-1])
#define jigds_arrfree(arr)        ((void) ((arr) ? jig_allocator_deallocate(JIGDS_ALLOCATOR, jigds_header(arr)) : (void)0), (arr)=NULL)

#endif
