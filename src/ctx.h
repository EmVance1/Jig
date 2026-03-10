#ifndef CJIG_IMPL_PARSECTX_H
#define CJIG_IMPL_PARSECTX_H
#include "lex.h"


typedef struct jigParseCtx {
    jigAllocator alloc;
    jigTokenStream tokens;
    jigError* errors;
    // vector
    jigToken* edges;
} jigParseCtx;


#endif
