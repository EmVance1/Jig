#include "lex.h"
#include <string.h>


static bool is_num(char c) {
    return '0' <= c && c <= '9';
}
static bool is_ident_begin(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}
static bool is_ident(char c) {
    return is_ident_begin(c) || is_num(c) || c == '.';
}
static bool is_space(char c, bool* wasnull) {
    if (c == '\0') *wasnull = true;
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\0';
}
static bool is_keyword(jigStrView str) {
#define STREQ(view, cptr) ((view.len == sizeof(cptr)-1) && (strncmp(view.ptr, cptr, view.len) == 0))
    if (STREQ(str, "if"))     return true;
    if (STREQ(str, "then"))   return true;
    if (STREQ(str, "else"))   return true;
    if (STREQ(str, "match"))  return true;
    if (STREQ(str, "with"))   return true;
    if (STREQ(str, "end"))    return true;
    if (STREQ(str, "true"))   return true;
    if (STREQ(str, "false"))  return true;
    if (STREQ(str, "inf"))    return true;
    if (STREQ(str, "rename")) return true;
    if (STREQ(str, "module")) return true;
    if (STREQ(str, "endmod")) return true;
    return false;
}


enum {
    LSTATE_VOID = 0,
    LSTATE_IDENT,
    LSTATE_NUMBER,
    LSTATE_STRING,
    LSTATE_STRINGESC,
    LSTATE_SYMBOL,
    LSTATE_COMMENT,
};

static jigToken _impl_step(jigTokenStream* self);

jigTokenStream jig_token_stream_init(const char* text) {
    jigTokenStream result = {
        .base=text,
        .ptr=text,
        .pos={ 1, 0 },
        .wasnull=false,
        .state=LSTATE_VOID,
        .cache={0},
    };
    result.cache = _impl_step(&result);
    return result;
}

jigToken jig_token_stream_peek(const jigTokenStream* self) {
    return self->cache;
}

jigToken jig_token_stream_next(jigTokenStream* self) {
    const jigToken temp = self->cache;
    self->cache = _impl_step(self);
    return temp;
}


static jigToken _impl_step(jigTokenStream* self) {
    const char* tbeg = NULL;
    size_t tlen = 0;
    jigErrorSpan tspan = self->pos;

    while (!self->wasnull) {
        jigErrorSpan p = self->pos;
        const char c = *(self->ptr++);
        if (c == '\n') { p.row++; p.col = 0; } else { p.col++; }

        switch (self->state) {
        case LSTATE_VOID:
            if (is_space(c, &self->wasnull)) {
                // continue
            } else if (is_ident_begin(c)) {
                self->state = LSTATE_IDENT;
                tbeg = self->ptr-1;
                tlen = 1;
                tspan = p;
            } else if (is_num(c)) {
                self->state = LSTATE_NUMBER;
                tbeg = self->ptr-1;
                tlen = 1;
                tspan = p;
            } else {
                tbeg = self->ptr-1;
                tlen = 1;
                tspan = p;
                self->pos = p;
                switch (c) {
                case '(': return (jigToken){ .type=JIG_TOKEN_OPENPAREN,  .value={ tbeg, 1 }, .span=tspan };
                case ')': return (jigToken){ .type=JIG_TOKEN_CLOSEPAREN, .value={ tbeg, 1 }, .span=tspan };
                case '[': return (jigToken){ .type=JIG_TOKEN_OPENBRACK,  .value={ tbeg, 1 }, .span=tspan };
                case ']': return (jigToken){ .type=JIG_TOKEN_CLOSEBRACK, .value={ tbeg, 1 }, .span=tspan };
                case '{': return (jigToken){ .type=JIG_TOKEN_OPENBRACE,  .value={ tbeg, 1 }, .span=tspan };
                case '}': return (jigToken){ .type=JIG_TOKEN_CLOSEBRACE, .value={ tbeg, 1 }, .span=tspan };
                case ',': return (jigToken){ .type=JIG_TOKEN_COMMA,      .value={ tbeg, 1 }, .span=tspan };
                case '+': case '-': case '*': case '/': case '<': case '>': case '=': case '!': case '&': case '|': case ':':
                    self->state = LSTATE_SYMBOL; break;
                case '"':
                    self->state = LSTATE_STRING; break;
                case '#':
                    self->state = LSTATE_COMMENT; break;
                default:
                    return (jigToken){ .type=JIG_TOKEN_ERROR, .value={ "unknown symbol encountered", 0 }, .span=tspan };
                }
            }
            break;

        case LSTATE_IDENT:
            if (is_ident(c)) {
                tlen++;
            } else {
                if (is_space(c, &self->wasnull)) {
                    self->pos = p;
                } else {
                    self->ptr--;
                }
                self->state = LSTATE_VOID;
                const jigStrView view = { tbeg, tlen };
                if (view.len == 1 && strncmp("_", view.ptr, 1) == 0) {
                    return (jigToken){ .type=JIG_TOKEN_CATCHALL, .value=view, .span=tspan };
                } else if (is_keyword(view)) {
                    return (jigToken){ .type=JIG_TOKEN_KEYWORD,  .value=view, .span=tspan };
                } else {
                    return (jigToken){ .type=JIG_TOKEN_IDENT,    .value=view, .span=tspan };
                }
            }
            break;

        case LSTATE_NUMBER:
            if (is_num(c)) {
                tlen++;
            } else if (is_space(c, &self->wasnull)) {
                self->pos = p;
                self->state = LSTATE_VOID;
                return (jigToken){ .type=JIG_TOKEN_NUMBER, .value={ tbeg, tlen }, .span=tspan };
            } else {
                self->ptr--;
                self->state = LSTATE_VOID;
                return (jigToken){ .type=JIG_TOKEN_NUMBER, .value={ tbeg, tlen }, .span=tspan };
            }
            break;

        case LSTATE_STRING:
            tlen++;
            if (c == '"') {
                self->pos = p;
                self->state = LSTATE_VOID;
                return (jigToken){ .type=JIG_TOKEN_STRLIT, .value={ tbeg+1, tlen-2 }, .span=tspan };
            } else if (c == '\\') {
                self->state = LSTATE_STRINGESC;
            }
            break;

        case LSTATE_STRINGESC:
            tlen++;
            self->state = LSTATE_STRING;
            break;

        case LSTATE_SYMBOL:
            self->state = LSTATE_VOID;
            if (c == '=') {
                self->pos = p;
                switch (*tbeg) {
                case '+': return (jigToken){ .type=JIG_TOKEN_ADDEQ, .value={ tbeg, 2 }, .span=tspan };
                case '-': return (jigToken){ .type=JIG_TOKEN_SUBEQ, .value={ tbeg, 2 }, .span=tspan };
                case '*': return (jigToken){ .type=JIG_TOKEN_MULEQ, .value={ tbeg, 2 }, .span=tspan };
                case '/': return (jigToken){ .type=JIG_TOKEN_DIVEQ, .value={ tbeg, 2 }, .span=tspan };
                case '<': return (jigToken){ .type=JIG_TOKEN_LEQ,   .value={ tbeg, 2 }, .span=tspan };
                case '>': return (jigToken){ .type=JIG_TOKEN_GEQ,   .value={ tbeg, 2 }, .span=tspan };
                case '=': return (jigToken){ .type=JIG_TOKEN_EQU,   .value={ tbeg, 2 }, .span=tspan };
                case '!': return (jigToken){ .type=JIG_TOKEN_NEQ,   .value={ tbeg, 2 }, .span=tspan };
                default:
                    return (jigToken){ .type=JIG_TOKEN_ERROR, .value={ "unknown double glyph encountered", 0 }, .span=tspan };
                }
            } else if (c == '&' && *tbeg == '&') {
                self->pos = p;
                return (jigToken){ .type=JIG_TOKEN_LOGAND, .value={ tbeg, 2 }, .span=tspan };
            } else if (c == '|' && *tbeg == '|') {
                self->pos = p;
                return (jigToken){ .type=JIG_TOKEN_LOGOR,  .value={ tbeg, 2 }, .span=tspan };
            } else if (c == ':' && *tbeg == ':') {
                self->pos = p;
                return (jigToken){ .type=JIG_TOKEN_JOIN,   .value={ tbeg, 2 }, .span=tspan };
            } else if (c == '>' && *tbeg == '=') {
                self->pos = p;
                return (jigToken){ .type=JIG_TOKEN_ARROW,  .value={ tbeg, 2 }, .span=tspan };
            } else {
                self->ptr--;
                switch (*tbeg) {
                case '+': return (jigToken){ .type=JIG_TOKEN_ADD,    .value={ tbeg, 1 }, .span=tspan };
                case '-': return (jigToken){ .type=JIG_TOKEN_SUB,    .value={ tbeg, 1 }, .span=tspan };
                case '*': return (jigToken){ .type=JIG_TOKEN_MUL,    .value={ tbeg, 1 }, .span=tspan };
                case '/': return (jigToken){ .type=JIG_TOKEN_DIV,    .value={ tbeg, 1 }, .span=tspan };
                case '<': return (jigToken){ .type=JIG_TOKEN_LTH,    .value={ tbeg, 1 }, .span=tspan };
                case '>': return (jigToken){ .type=JIG_TOKEN_GTH,    .value={ tbeg, 1 }, .span=tspan };
                case '=': return (jigToken){ .type=JIG_TOKEN_SETEQ,  .value={ tbeg, 1 }, .span=tspan };
                case '!': return (jigToken){ .type=JIG_TOKEN_LOGNOT, .value={ tbeg, 1 }, .span=tspan };
                case ':': return (jigToken){ .type=JIG_TOKEN_COLON,  .value={ tbeg, 1 }, .span=tspan };
                case '&': case '|':
                    return (jigToken){ .type=JIG_TOKEN_ERROR,   .value={ "unknown double glyph encountered", 0 }, .span=tspan };
                default:
                    return (jigToken){ .type=JIG_TOKEN_MYFAULT, .value={ "CODING ERROR: switch should never default", 0 }, .span=tspan };
                }
            }
            break;

        case LSTATE_COMMENT:
            if (c == '\n') {
                self->state = LSTATE_VOID;
            }
            break;
        }
        self->pos = p;
    }

    return (jigToken){ .type=JIG_TOKEN_EOF, .value={ "[[EOF]]", 0 }, .span=self->pos };
}

