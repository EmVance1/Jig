#include "quosi/quosi.h"
#include "quosi/ast.h"
#include "lex.h"
#include "expr.h"
#include "error.h"
#include <string.h>
#define QUOSIDS_ALLOCATOR (ctx->alloc)
#include "vec.h"


#define TNEXT(stream)  quosi_token_stream_next(stream)
#define TPEEK(stream)  quosi_token_stream_peek(stream)
#define STREQ(view, cptr) ((view.len == sizeof(cptr)-1) && (strncmp(view.ptr, cptr, view.len) == 0))

static void parse_graph(quosiParseCtx* ctx, quosiGraph* result);
static void parse_vert(quosiParseCtx* ctx, quosiVertex* result);
static void parse_vert_if(quosiParseCtx* ctx, quosiVertexIfElse* result);
static void parse_vert_if_body(quosiParseCtx* ctx, quosiVertexBlock* result);
static void parse_vert_match(quosiParseCtx* ctx, quosiVertexMatch* result);
static void parse_edge(quosiParseCtx* ctx, quosiEdge* result);
static void parse_edge_if(quosiParseCtx* ctx, quosiEdgeIfElse* result);
static void parse_edge_if_body(quosiParseCtx* ctx, quosiEdgeBlock** result, bool top);
static void parse_edge_match(quosiParseCtx* ctx, quosiEdgeMatch* result);
static void parse_effect(quosiParseCtx* ctx, quosiEffect** result);

static bool contains_key(quosiNamedVertex* verts, quosiStrView str) {
    for (size_t i = 0; i < quosids_arrlenu(verts); i++) {
        if (verts[i].name.len == str.len && strncmp(verts[i].name.ptr, str.ptr, str.len) == 0) {
            return true;
        }
    }
    return false;
}


quosiAst quosi_ast_parse_from_src(const char* src, quosiError* errors, quosiAllocator alloc) {
    quosiParseCtx context = {
        .alloc=alloc,
        .tokens=quosi_token_stream_init(src),
        .errors=errors,
        .edges=NULL,
    };
    quosiParseCtx* ctx = &context;
    quosiAst result = { 0 };

    quosiToken n = TNEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_EOF) {
        // module NAME
        EH_CHECK_RET(n, KEYWORD, BAD_GRAPH_BEGIN);
        if (!STREQ(n.value, "module")) EH_FAIL_RET(n, BAD_GRAPH_BEGIN);
        const quosiToken name = TNEXT(&ctx->tokens);
        EH_CHECK_RET(name, IDENT, BAD_GRAPH_BEGIN);

        quosids_arrpush(result.modules, (quosiGraph){ .name=name.value });
        quosiGraph* graph = &quosids_arrlast(result.modules);
        parse_graph(ctx, graph);
        EH_PROP_RET();
        n = TNEXT(&ctx->tokens);
    }

    return result;
}

static void parse_graph(quosiParseCtx* ctx, quosiGraph* result) {
    quosiToken n = TNEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_EOF) {
        // rename INIT => NEW
        if (n.type == QUOSI_TOKEN_KEYWORD) {
            if (STREQ(n.value, "endmod")) {
                goto happypath;
            } else if (!STREQ(n.value, "rename")) {
                EH_FAIL(n, BAD_VERTEX_BEGIN);
            }
            const quosiToken init = TNEXT(&ctx->tokens); // n = <INIT>
            EH_CHECK(init, IDENT, BAD_RENAME);
            n = TNEXT(&ctx->tokens);
            EH_CHECK(n, ARROW, BAD_RENAME);
            const quosiToken rend = TNEXT(&ctx->tokens); // n = <NEW>
            EH_CHECK(rend, IDENT, BAD_RENAME);
            // result.rename_table.emplace(rend.value, init.value);
            n = TNEXT(&ctx->tokens); // FOLLOW(rename)
            continue;
        }
        // IDENT =
        EH_CHECK(n, IDENT, BAD_VERTEX_BEGIN);
        const quosiToken name = n;
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, SETEQ, MISPLACED_TOKEN);

        if (contains_key(result->vertices, name.value)) EH_FAIL(name, DUPLICATE_VERTEX);
        quosids_arrpush(result->vertices, ((quosiNamedVertex){ .name=name.value }));
        quosiVertexBlock* vert = &quosids_arrlast(result->vertices).data;
        parse_vert_if_body(ctx, vert);
        EH_PROP();
        n = TNEXT(&ctx->tokens);
    }

    EH_FAIL(n, EARLY_EOF);

happypath:
    if (!contains_key(result->vertices, (quosiStrView){ .ptr="START", .len=sizeof("START")-1 })) EH_FAIL(n, NO_ENTRY);
    for (size_t i = 0; i < quosids_arrlenu(ctx->edges); i++) {
        const quosiToken edge = ctx->edges[i];
        if (STREQ(edge.value, "START") || STREQ(edge.value, "EXIT")) continue;
        if (!contains_key(result->vertices, edge.value)) {
             EH_FAIL(edge, DANGLING_EDGE);
        }
    }

    quosids_arrfree(ctx->edges);
}

static void parse_vert(quosiParseCtx* ctx, quosiVertex* result) {
    quosiToken n = TNEXT(&ctx->tokens);
    while (n.type == QUOSI_TOKEN_LTH) {
        // <IDENT:
        quosids_arrpush(result->lineset, (quosiVertexLineSet){ 0 });
        quosiVertexLineSet* lines = &quosids_arrlast(result->lineset);
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, IDENT, UNKNOWN);
        lines->speaker = n.value;
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, COLON, UNKNOWN);

        // STRING,... >
        n = TNEXT(&ctx->tokens);
        while (n.type != QUOSI_TOKEN_GTH) {
            EH_CHECK(n, STRLIT, UNKNOWN);
            quosids_arrpush(lines->lines, n.value);
            n = TNEXT(&ctx->tokens);
            switch (n.type) {
            case QUOSI_TOKEN_COMMA:
                n = TNEXT(&ctx->tokens);
                break;
            case QUOSI_TOKEN_GTH:
                goto endlines;

            default:
                EH_FAIL(n, UNCLOSED_ANGLE);
            }
        }
endlines:
        n = TNEXT(&ctx->tokens);
    }

    switch (n.type) {
    case QUOSI_TOKEN_JOIN:
        // :: (EFFS) => IDENT
        result->type = QUOSI_VERTEX_JUMP;
        parse_effect(ctx, &result->v.jump.effects);
        EH_PROP();
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, ARROW, UNKNOWN);
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, IDENT, UNKNOWN);
        result->v.jump.next = n.value;
        quosids_arrpush(ctx->edges, n);
        break;

    case QUOSI_TOKEN_ARROW:
        // => IDENT
        result->type = QUOSI_VERTEX_JUMP;
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, IDENT, UNKNOWN);
        result->v.jump.next = n.value;
        quosids_arrpush(ctx->edges, n);
        break;
    case QUOSI_TOKEN_OPENPAREN:
        // ( ... )
        parse_edge_if_body(ctx, &result->v.edges, true);
        EH_PROP();
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, CLOSEPAREN, UNCLOSED_PAREN);
        break;

    default:
        EH_FAIL(n, UNKNOWN);
    }

    return;
}
static void parse_vert_if(quosiParseCtx* ctx, quosiVertexIfElse* result) {
    quosiToken n = TNEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_EOF) {
        quosids_arrpush(result->blocks, (quosiVertexIfElseBlock){ 0 });
        quosiVertexIfElseBlock* block = &quosids_arrlast(result->blocks);

        // if (EXPR) then
        n = TPEEK(&ctx->tokens);
        EH_CHECK(n, OPENPAREN, UNKNOWN);
        quosi_internal_parse_expr(ctx, &block->cond);
        EH_PROP();
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, KEYWORD, UNKNOWN);
        if (!STREQ(n.value, "then")) EH_FAIL(n, UNKNOWN);

        block->data = quosi_allocator_allocate(ctx->alloc, sizeof(quosiVertexBlock));
        *block->data = (quosiVertexBlock){ 0 };
        parse_vert_if_body(ctx, block->data);
        EH_PROP();

        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, KEYWORD, UNKNOWN);
        if (STREQ(n.value, "else")) {
            if (STREQ(TPEEK(&ctx->tokens).value, "if")) {
                TNEXT(&ctx->tokens);
                continue;
            } else {
                result->catchall = quosi_allocator_allocate(ctx->alloc, sizeof(quosiVertexBlock));
                *result->catchall = (quosiVertexBlock){ 0 };
                parse_vert_if_body(ctx, result->catchall);
                n = TNEXT(&ctx->tokens);
                EH_CHECK(n, KEYWORD, UNCLOSED_CONDITIONAL);
                if (!STREQ(n.value, "end")) EH_FAIL(n, UNCLOSED_CONDITIONAL);
                return;
            }
        } else if (STREQ(n.value, "end")) {
            EH_FAIL(n, NO_ELSE);
            return;
        }
    }

    EH_FAIL(n, EARLY_EOF);
}
static void parse_vert_if_body(quosiParseCtx* ctx, quosiVertexBlock* result) {
    quosiToken n = TPEEK(&ctx->tokens);
    switch (n.type) {
    case QUOSI_TOKEN_KEYWORD:
        if (STREQ(n.value, "match")) {
            result->tag = QUOSI_VBLOCK_MATCH;
            parse_vert_match(ctx, &result->value.match);
        } else if (STREQ(n.value, "if")) {
            result->tag = QUOSI_VBLOCK_IFELSE;
            parse_vert_if(ctx, &result->value.ifelse);
        }
        break;
    case QUOSI_TOKEN_LTH:
        result->tag = QUOSI_VBLOCK_T;
        parse_vert(ctx, &result->value.vertex);
        break;

    default:
        EH_FAIL(n, BAD_VERTEX_BLOCK);
    }

    return;
}
static void parse_vert_match(quosiParseCtx* ctx, quosiVertexMatch* result) {
    quosiToken n = TNEXT(&ctx->tokens);
    bool has_catchall = false;

    // match (EXPR) with
    n = TPEEK(&ctx->tokens);
    EH_CHECK(n, OPENPAREN, UNKNOWN);
    quosi_internal_parse_expr(ctx, &result->expr);
    EH_PROP();
    n = TNEXT(&ctx->tokens);
    EH_CHECK(n, KEYWORD, UNKNOWN);
    if (!STREQ(n.value, "with")) EH_FAIL(n, UNKNOWN);

    n = TNEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_EOF) {
        switch (n.type) {
        case QUOSI_TOKEN_KEYWORD:
            if (!STREQ(n.value, "end")) EH_FAIL(n, UNCLOSED_CONDITIONAL);
            if (!has_catchall)          EH_FAIL(n, NO_CATCHALL);
            return;

        case QUOSI_TOKEN_OPENPAREN: {
            quosiVertex* arm_vert;
            if (TPEEK(&ctx->tokens).type == QUOSI_TOKEN_CATCHALL) {
                TNEXT(&ctx->tokens);
                if (has_catchall) EH_FAIL(n, DUPLICATE_CASE);
                arm_vert = &result->catchall;
                has_catchall = true;
            } else {
                quosids_arrpush(result->arms, (quosiVertexMatchArm){ 0 });
                quosiVertexMatchArm* arm = &quosids_arrlast(result->arms);
                quosi_internal_parse_value(ctx, &arm->cond);
                EH_PROP();
                arm_vert = &arm->body;
            }
            n = TNEXT(&ctx->tokens);
            EH_CHECK(n, CLOSEPAREN, UNCLOSED_PAREN);
            n = TPEEK(&ctx->tokens);
            EH_CHECK(n, LTH, UNKNOWN);
            parse_vert(ctx, arm_vert);
            EH_PROP();
            break; }

        default:
            EH_FAIL(n, BAD_MATCH_ARM);
            break;
        }
        n = TNEXT(&ctx->tokens);
    }

    EH_FAIL(n, EARLY_EOF);
}

static void parse_edge(quosiParseCtx* ctx, quosiEdge* result) {
    // STRING :: (EFFECT) => IDENT
    quosiToken n = TNEXT(&ctx->tokens);
    EH_CHECK(n, STRLIT, UNKNOWN);
    result->line = n.value;
    n = TNEXT(&ctx->tokens);
    switch (n.type) {
    case QUOSI_TOKEN_JOIN:
        parse_effect(ctx, &result->effects);
        EH_PROP();
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, ARROW, UNKNOWN);
        break;

    case QUOSI_TOKEN_ARROW:
        break;

    default:
        EH_FAIL(n, UNKNOWN);
    }
    n = TNEXT(&ctx->tokens);
    EH_CHECK(n, IDENT, UNKNOWN);
    result->next = n.value;
    quosids_arrpush(ctx->edges, n);
}
static void parse_edge_if(quosiParseCtx* ctx, quosiEdgeIfElse* result) {
    quosiToken n = TNEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_EOF) {
        quosids_arrpush(result->blocks, (quosiEdgeIfElseBlock){ 0 });
        quosiEdgeIfElseBlock* block = &quosids_arrlast(result->blocks);

        // if (EXPR) then
        n = TPEEK(&ctx->tokens);
        EH_CHECK(n, OPENPAREN, UNKNOWN);
        quosi_internal_parse_expr(ctx, &block->cond);
        EH_PROP();
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, KEYWORD, UNKNOWN);
        if (!STREQ(n.value, "then")) EH_FAIL(n, UNKNOWN);

        parse_edge_if_body(ctx, &block->body, false);
        EH_PROP();

        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, KEYWORD, UNKNOWN);
        if (STREQ(n.value, "else")) {
            if (STREQ(TPEEK(&ctx->tokens).value, "if")) {
                TNEXT(&ctx->tokens);
                continue;
            } else {
                parse_edge_if_body(ctx, &result->catchall, false);
                EH_PROP();
                n = TNEXT(&ctx->tokens);
                EH_CHECK(n, KEYWORD, UNCLOSED_CONDITIONAL);
                if (!STREQ(n.value, "end")) EH_FAIL(n, UNCLOSED_CONDITIONAL);
                return;
            }
        } else if (STREQ(n.value, "end")) {
            return;
        }
    }

    EH_FAIL(n, EARLY_EOF);
}
static void parse_edge_if_body(quosiParseCtx* ctx, quosiEdgeBlock** result, bool top) {
    bool last_was_edge = false;
    quosiToken n = TPEEK(&ctx->tokens);

    while (n.type != QUOSI_TOKEN_EOF) {
        n = TPEEK(&ctx->tokens);
        switch (n.type) {
        case QUOSI_TOKEN_CLOSEPAREN:
            if (top) {
                return;
            } else {
                EH_FAIL(n, UNKNOWN);
            }
            break;
        case QUOSI_TOKEN_KEYWORD:
            if (STREQ(n.value, "match")) {
                quosids_arrpush(*result, (quosiEdgeBlock){ .tag=QUOSI_EBLOCK_MATCH });
                quosiEdgeBlock* ref = &quosids_arrlast(*result);
                parse_edge_match(ctx, &ref->value.match);
            } else if (STREQ(n.value, "if")) {
                quosids_arrpush(*result, (quosiEdgeBlock){ .tag=QUOSI_EBLOCK_IFELSE });
                quosiEdgeBlock* ref = &quosids_arrlast(*result);
                parse_edge_if(ctx, &ref->value.ifelse);
            } else if (STREQ(n.value, "else") || STREQ(n.value, "end")) {
                if (!top) {
                    return;
                } else {
                    EH_FAIL(n, UNKNOWN);
                }
            }
            last_was_edge = false;
            break;
        case QUOSI_TOKEN_STRLIT:
            if (!last_was_edge) {
                quosids_arrpush(*result, (quosiEdgeBlock){ .tag=QUOSI_EBLOCK_T });
            }
            quosiEdgeBlock* ref = &quosids_arrlast(*result);
            quosids_arrpush(ref->value.edges, (quosiEdge){ 0 });
            quosiEdge* edge = &quosids_arrlast(ref->value.edges);
            parse_edge(ctx, edge);
            last_was_edge = true;
            break;

        default:
            EH_FAIL(n, BAD_EDGE_BLOCK);
        }

        EH_PROP();
    }

    EH_FAIL(n, EARLY_EOF);
}
static void parse_edge_match(quosiParseCtx* ctx, quosiEdgeMatch* result) {
    quosiToken n = TNEXT(&ctx->tokens);

    // match (EXPR) with
    n = TPEEK(&ctx->tokens);
    EH_CHECK(n, OPENPAREN, UNKNOWN);
    quosi_internal_parse_expr(ctx, &result->expr);
    EH_PROP();
    n = TNEXT(&ctx->tokens);
    EH_CHECK(n, KEYWORD, UNKNOWN);
    if (!STREQ(n.value, "with")) EH_FAIL(n, UNKNOWN);

    n = TNEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_EOF) {
        switch (n.type) {
        case QUOSI_TOKEN_KEYWORD:
            if (!STREQ(n.value, "end")) EH_FAIL(n, UNCLOSED_CONDITIONAL);
            return;

        case QUOSI_TOKEN_OPENPAREN: {
            quosiEdge* arm_es;
            if (TPEEK(&ctx->tokens).type == QUOSI_TOKEN_CATCHALL) {
                if (result->catchall.exists) EH_FAIL(n, DUPLICATE_CASE);
                TNEXT(&ctx->tokens);
                result->catchall.exists = true;
                arm_es = &result->catchall.arm;
            } else {
                quosids_arrpush(result->arms, (quosiEdgeMatchArm){ 0 });
                quosiEdgeMatchArm* arm = &quosids_arrlast(result->arms);
                quosi_internal_parse_value(ctx, &arm->cond);
                arm_es = &arm->body;
            }
            n = TNEXT(&ctx->tokens);
            EH_CHECK(n, CLOSEPAREN, UNCLOSED_PAREN);
            n = TPEEK(&ctx->tokens);
            EH_CHECK(n, STRLIT, UNKNOWN);
            arm_es->effects = NULL;
            parse_edge(ctx, arm_es);
            EH_PROP();
            break; }

        default:
            EH_FAIL(n, UNKNOWN);
            break;
        }
        n = TNEXT(&ctx->tokens);
    }

    EH_FAIL(n, EARLY_EOF);
}
static void parse_effect(quosiParseCtx* ctx, quosiEffect** result) {
    quosiToken n = TNEXT(&ctx->tokens);
    EH_CHECK(n, OPENPAREN, UNKNOWN);

    n = TNEXT(&ctx->tokens);
    while (n.type != QUOSI_TOKEN_CLOSEPAREN) {
        EH_CHECK(n, IDENT, INVALID_ASSIGN);
        quosids_arrpush(*result, ((quosiEffect){ .lhs=n.value, .op=QUOSI_EFFECT_EVENT }));
        quosiEffect* effect = &quosids_arrlast(*result);

        n = TNEXT(&ctx->tokens);
        switch (n.type) {
        case QUOSI_TOKEN_ADDEQ: case QUOSI_TOKEN_SUBEQ: case QUOSI_TOKEN_MULEQ: case QUOSI_TOKEN_DIVEQ: case QUOSI_TOKEN_SETEQ:
            switch (n.type) {
            case QUOSI_TOKEN_ADDEQ:
                effect->op = QUOSI_EFFECT_ADD; break;
            case QUOSI_TOKEN_SUBEQ:
                effect->op = QUOSI_EFFECT_SUB; break;
            case QUOSI_TOKEN_MULEQ:
                effect->op = QUOSI_EFFECT_MUL; break;
            case QUOSI_TOKEN_DIVEQ:
                effect->op = QUOSI_EFFECT_DIV; break;
            case QUOSI_TOKEN_SETEQ:
                effect->op = QUOSI_EFFECT_SET; break;
            default:
                EH_FAIL(n, UNREACHABLE);
                break;
            }
            quosi_internal_parse_expr(ctx, &effect->rhs);
            EH_PROP();
            n = TNEXT(&ctx->tokens);
            switch (n.type) {
            case QUOSI_TOKEN_COMMA:
                n = TNEXT(&ctx->tokens);
                break;
            case QUOSI_TOKEN_CLOSEPAREN:
                break;

            default:
                EH_FAIL(n, UNCLOSED_PAREN);
            }
            break;

        case QUOSI_TOKEN_COMMA:
            n = TNEXT(&ctx->tokens);
            break;
        case QUOSI_TOKEN_CLOSEPAREN:
            return;

        default:
            EH_FAIL(n, INVALID_OPERATOR);
        }
    }
}

