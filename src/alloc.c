#include <stddef.h>
#ifdef _WIN32
#include <Windows.h>
#else
#define _GNU_SOURCE
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "jig/jig.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


static size_t system_page_size(void) {
    static size_t size = SIZE_MAX;
    if (size == SIZE_MAX) {
#ifdef _WIN32
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        size = info.dwPageSize;
#else
        size = (size_t)sysconf(_SC_PAGESIZE);
#endif
    }
    return size;
}

#define ROUND(x, n) ( ((x)+(n)-1) / (n) * (n) )

static void _arena_grow(jigMemoryArena* self, size_t minsize);

jigMemoryArena jig_memory_arena_create(int source, size_t init) {
    const size_t blocksize = init == 0 ? 10 * system_page_size() : ROUND(init, system_page_size());
    jigMemoryArenaConfig cfg = {
        .initial=blocksize,
        .max_reserve=1000*1000*1000,
        .source=source,
        .lazyinit=init==0,
        .growth_policy=JIG_MEMORY_ARENA_GEOMETRIC,
    };
    return jig_memory_arena_createex(cfg);
}

jigMemoryArena jig_memory_arena_createex(jigMemoryArenaConfig cfg) {
    const size_t blocksize = ROUND(cfg.initial, system_page_size());

    jigMemoryArena result = {
        .base_ptr=NULL,
        .curr_ptr=NULL,
        .last_ptr=NULL,
        .reserve=cfg.max_reserve,
        .blocksize=blocksize,
        .commit=0,
        .source=cfg.source,
    };

    if (cfg.source == JIG_MEMORY_ARENA_PAGE) {
#ifdef _WIN32
        result.base_ptr = VirtualAlloc(NULL, cfg.max_reserve, MEM_RESERVE, PAGE_READWRITE);
        if (!result.base_ptr) return (jigMemoryArena){ 0 };
#else
        result.base_ptr = mmap(NULL, cfg.max_reserve, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (result.base_ptr == MAP_FAILED) return (jigMemoryArena){ 0 };
#endif
    }

    result.curr_ptr = result.base_ptr;
    if (!cfg.lazyinit) _arena_grow(&result, blocksize);
    return result;
}

void jig_memory_arena_destroy(jigMemoryArena* self) {
    if (!self->base_ptr) return;
    if (self->source == JIG_MEMORY_ARENA_PAGE) {
#ifdef _WIN32
        VirtualFree(self->base_ptr, 0, MEM_RELEASE);
#else
        munmap(self->base_ptr, self->reserve);
#endif
    } else {
        free(self->base_ptr);
    }

    self->base_ptr = NULL;
    self->curr_ptr = NULL;
    self->last_ptr = NULL;
    self->reserve = 0;
    self->commit = 0;
}

void jig_memory_arena_reset(jigMemoryArena* self) {
    if (self->source == JIG_MEMORY_ARENA_PAGE && self->commit > self->blocksize) {
        const size_t rem = self->commit - self->blocksize;

#ifdef _WIN32
        VirtualFree(self->base_ptr + self->blocksize, rem, MEM_DECOMMIT);
#else
        madvise(self->base_ptr + self->blocksize, rem, MADV_DONTNEED);
        mprotect(self->base_ptr + self->blocksize, rem, PROT_NONE);
#endif
    }

    self->curr_ptr = self->base_ptr;
    self->last_ptr = NULL;
    self->commit = self->blocksize;
}


static void _arena_grow(jigMemoryArena* self, size_t minsize) {
    size_t newsize = self->commit + ((self->commit == 0 || self->policy == JIG_MEMORY_ARENA_LINEAR) ? self->blocksize : self->commit);
    if (minsize > newsize) {
        newsize = ROUND(minsize, system_page_size());
    }
    const size_t growth = newsize - self->commit;

    if (self->source == JIG_MEMORY_ARENA_PAGE) {
#ifdef _WIN32
        if (!VirtualAlloc(self->base_ptr + self->commit, growth, MEM_COMMIT, PAGE_READWRITE)) abort();
#else
        if (mprotect(self->base_ptr + self->commit, growth, PROT_READ | PROT_WRITE) != 0) abort();
#endif
    } else if (self->commit == 0) {
        self->base_ptr = malloc(growth);
        self->curr_ptr = self->base_ptr;
    } else {
        abort();
    }

    self->commit += growth;
}

static void* _arena_allocate(void* _self, size_t nbytes) {
    if (nbytes == 0) return NULL;
    jigMemoryArena* self = (jigMemoryArena*)_self;
    const size_t block = ROUND(nbytes, 16);
    const size_t used = (size_t)(self->curr_ptr - self->base_ptr);
    if (used + block > self->commit) {
        _arena_grow(_self, used + block);
    }
    uint8_t* ptr = self->curr_ptr;
    self->curr_ptr += block;
    self->last_ptr = ptr;
    return ptr;
}
static void _arena_deallocate(void* _self, void* ptr) {
    if (ptr == NULL) return;
    jigMemoryArena* self = (jigMemoryArena*)_self;
    if (ptr == self->last_ptr) {
        self->curr_ptr = self->last_ptr;
        self->last_ptr = NULL;
    }
}
static void* _arena_reallocate(void* _self, void* oldptr, size_t oldlen, size_t newlen) {
    if (newlen == 0) return NULL;
    if (oldptr == NULL) return NULL;
    jigMemoryArena* self = (jigMemoryArena*)_self;
    void* newptr;
    if (oldptr == self->last_ptr) {
        self->curr_ptr = self->last_ptr;
        const size_t block = ROUND(newlen, 16);
        const size_t used = (size_t)(self->curr_ptr - self->base_ptr);
        if (used + block > self->commit) {
            _arena_grow(_self, used + block);
        }
        newptr = self->curr_ptr;
        self->curr_ptr += block;
    } else {
        newptr = _arena_allocate(_self, newlen);
        memcpy(newptr, oldptr, newlen < oldlen ? newlen : oldlen);
    }
    self->last_ptr = newptr;
    return newptr;
}

jigAllocator jig_memory_arena_allocator(jigMemoryArena* arena) {
    return (jigAllocator){
        .impl=arena,
        .allocate  =_arena_allocate,
        .deallocate=_arena_deallocate,
        .reallocate=_arena_reallocate,
    };
}


static void* _malloc_allocate(void* _self, size_t nbytes) {
    (void)_self;
    return malloc(nbytes);
}
static void _malloc_deallocate(void* _self, void* ptr) {
    (void)_self;
    free(ptr);
}
static void* _malloc_reallocate(void* _self, void* oldptr, size_t oldbytes, size_t newbytes) {
    (void)_self;
    (void)oldbytes;
    return realloc(oldptr, newbytes);
}

jigAllocator jig_malloc_allocator(void) {
    return (jigAllocator){
        .impl=NULL,
        .allocate  =_malloc_allocate,
        .deallocate=_malloc_deallocate,
        .reallocate=_malloc_reallocate,
    };
}


void* jig_allocator_allocate(jigAllocator self, size_t nbytes) {
    return self.allocate(self.impl, nbytes);
}
void jig_allocator_deallocate(jigAllocator self, void* ptr) {
    self.deallocate(self.impl, ptr);
}
void* jig_allocator_reallocate(jigAllocator self, void* oldptr, size_t oldbytes, size_t newbytes) {
    return self.reallocate(self.impl, oldptr, oldbytes, newbytes);
}

