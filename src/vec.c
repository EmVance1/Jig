#include "vec.h"


void* jigds_arrgrowf(void* a, size_t elemsize, size_t addlen, size_t min_cap, jigAllocator alloc) {
    static const jigDynArrayHeader empty = { 0 };
    const jigDynArrayHeader* header = a ? jigds_header(a) : &empty;
    const size_t min_len = header->len + addlen;

    if (min_len > min_cap) {
        min_cap = min_len;
    }
    if (min_cap <= header->cap) {
        return a;
    }

    if (min_cap < 2 * header->cap) {
        min_cap = 2 * header->cap;
    } else if (min_cap < JIGDS_INIT_LEN) {
        min_cap = JIGDS_INIT_LEN;
    }

    jigDynArrayHeader* b;
    if (a == NULL) {
        b = jig_allocator_allocate(alloc, elemsize * min_cap + sizeof(jigDynArrayHeader));
        b->cap = min_cap;
        b->len = 0;
    } else {
        const size_t oldcap = elemsize * header->cap + sizeof(jigDynArrayHeader);
        b = jig_allocator_reallocate(alloc, jigds_header(a), oldcap, elemsize * min_cap + sizeof(jigDynArrayHeader));
        b->cap = min_cap;
    }
    return b + 1;
}

