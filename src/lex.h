#ifndef CJIG_IMPL_LEX_H
#define CJIG_IMPL_LEX_H
#include "jig/jig.h"
#include <stdbool.h>
#include <stdint.h>


typedef struct jigToken {
    enum jigTokenType {
        JIG_TOKEN_MYFAULT = -2,
        JIG_TOKEN_ERROR = -1,
        JIG_TOKEN_NONE = 0,

        JIG_TOKEN_OPENPAREN,
        JIG_TOKEN_CLOSEPAREN,
        JIG_TOKEN_OPENBRACK,
        JIG_TOKEN_CLOSEBRACK,
        JIG_TOKEN_OPENBRACE,
        JIG_TOKEN_CLOSEBRACE,

        JIG_TOKEN_ADD,
        JIG_TOKEN_SUB,
        JIG_TOKEN_MUL,
        JIG_TOKEN_DIV,

        JIG_TOKEN_EQU,
        JIG_TOKEN_NEQ,
        JIG_TOKEN_LTH,
        JIG_TOKEN_GTH,
        JIG_TOKEN_LEQ,
        JIG_TOKEN_GEQ,

        JIG_TOKEN_LOGNOT,
        JIG_TOKEN_LOGAND,
        JIG_TOKEN_LOGOR,

        JIG_TOKEN_ADDEQ,
        JIG_TOKEN_SUBEQ,
        JIG_TOKEN_MULEQ,
        JIG_TOKEN_DIVEQ,
        JIG_TOKEN_SETEQ,

        JIG_TOKEN_ARROW,
        JIG_TOKEN_COMMA,
        JIG_TOKEN_COLON,
        JIG_TOKEN_JOIN,
        JIG_TOKEN_CATCHALL,

        JIG_TOKEN_IDENT,
        JIG_TOKEN_NUMBER,
        JIG_TOKEN_STRLIT,
        JIG_TOKEN_KEYWORD,

        JIG_TOKEN_EOF,
    } type;
    jigStrView value;
    jigErrorSpan span;
} jigToken;

typedef struct jigTokenStream {
    const char* base;
    const char* ptr;
    jigErrorSpan pos;
    bool wasnull;
    int state;
    jigToken cache;
} jigTokenStream;

jigTokenStream jig_token_stream_init(const char* text);
jigToken jig_token_stream_peek(const jigTokenStream* self);
jigToken jig_token_stream_next(jigTokenStream* self);


#endif
