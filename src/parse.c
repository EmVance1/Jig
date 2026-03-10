#include "jig/jig.h"
#include "jig/ast.h"
#include "lex.h"
#include "expr.h"
#include "error.h"
#include <string.h>
#define JIGDS_ALLOCATOR (ctx->alloc)
#include "vec.h"


#define TNEXT(stream)  jig_token_stream_next(stream)
#define TPEEK(stream)  jig_token_stream_peek(stream)
#define STREQ(view, cptr) ((view.len == sizeof(cptr)-1) && (strncmp(view.ptr, cptr, view.len) == 0))

static void parse_graph(jigParseCtx* ctx, jigGraph* result);
static void parse_vert(jigParseCtx* ctx, jigVertex* result);
static void parse_vert_if(jigParseCtx* ctx, jigVertexIfElse* result);
static void parse_vert_if_body(jigParseCtx* ctx, jigVertexBlock* result);
static void parse_vert_match(jigParseCtx* ctx, jigVertexMatch* result);
static void parse_edge(jigParseCtx* ctx, jigEdge* result);
static void parse_edge_if(jigParseCtx* ctx, jigEdgeIfElse* result);
static void parse_edge_if_body(jigParseCtx* ctx, jigEdgeBlock** result, bool top);
static void parse_edge_match(jigParseCtx* ctx, jigEdgeMatch* result);
static void parse_effect(jigParseCtx* ctx, jigEffect** result);

static bool contains_key(jigNamedVertex* verts, jigStrView str) {
    for (size_t i = 0; i < jigds_arrlenu(verts); i++) {
        if (verts[i].name.len == str.len && strncmp(verts[i].name.ptr, str.ptr, str.len) == 0) {
            return true;
        }
    }
    return false;
}


jigAst jig_ast_parse_from_src(const char* src, jigError* errors, jigAllocator alloc) {
    jigParseCtx context = {
        .alloc=alloc,
        .tokens=jig_token_stream_init(src),
        .errors=errors,
        .edges=NULL,
    };
    jigParseCtx* ctx = &context;
    jigAst result = { 0 };

    jigToken n = TNEXT(&ctx->tokens);
    while (n.type != JIG_TOKEN_EOF) {
        // module NAME
        EH_CHECK_RET(n, KEYWORD, BAD_GRAPH_BEGIN);
        if (!STREQ(n.value, "module")) EH_FAIL_RET(n, BAD_GRAPH_BEGIN);
        const jigToken name = TNEXT(&ctx->tokens);
        EH_CHECK_RET(name, IDENT, BAD_GRAPH_BEGIN);

        jigds_arrpush(result.modules, (jigGraph){ .name=name.value });
        jigGraph* graph = &jigds_arrlast(result.modules);
        parse_graph(ctx, graph);
        EH_PROP_RET();
        n = TNEXT(&ctx->tokens);
    }

    return result;
}

static void parse_graph(jigParseCtx* ctx, jigGraph* result) {
    jigToken n = TNEXT(&ctx->tokens);
    while (n.type != JIG_TOKEN_EOF) {
        // rename INIT => NEW
        if (n.type == JIG_TOKEN_KEYWORD) {
            if (STREQ(n.value, "endmod")) {
                goto happypath;
            } else if (!STREQ(n.value, "rename")) {
                EH_FAIL(n, BAD_VERTEX_BEGIN);
            }
            const jigToken init = TNEXT(&ctx->tokens); // n = <INIT>
            EH_CHECK(init, IDENT, BAD_RENAME);
            n = TNEXT(&ctx->tokens);
            EH_CHECK(n, ARROW, BAD_RENAME);
            const jigToken rend = TNEXT(&ctx->tokens); // n = <NEW>
            EH_CHECK(rend, IDENT, BAD_RENAME);
            // result.rename_table.emplace(rend.value, init.value);
            n = TNEXT(&ctx->tokens); // FOLLOW(rename)
            continue;
        }
        // IDENT =
        EH_CHECK(n, IDENT, BAD_VERTEX_BEGIN);
        const jigToken name = n;
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, SETEQ, MISPLACED_TOKEN);

        if (contains_key(result->vertices, name.value)) EH_FAIL(name, DUPLICATE_VERTEX);
        jigds_arrpush(result->vertices, ((jigNamedVertex){ .name=name.value }));
        jigVertexBlock* vert = &jigds_arrlast(result->vertices).data;
        parse_vert_if_body(ctx, vert);
        EH_PROP();
        n = TNEXT(&ctx->tokens);
    }

    EH_FAIL(n, EARLY_EOF);

happypath:
    if (!contains_key(result->vertices, (jigStrView){ .ptr="START", .len=sizeof("START")-1 })) EH_FAIL(n, NO_ENTRY);
    for (size_t i = 0; i < jigds_arrlenu(ctx->edges); i++) {
        const jigToken edge = ctx->edges[i];
        if (STREQ(edge.value, "START") || STREQ(edge.value, "EXIT")) continue;
        if (!contains_key(result->vertices, edge.value)) {
             EH_FAIL(edge, DANGLING_EDGE);
        }
    }

    jigds_arrfree(ctx->edges);
}

static void parse_vert(jigParseCtx* ctx, jigVertex* result) {
    jigToken n = TNEXT(&ctx->tokens);
    while (n.type == JIG_TOKEN_LTH) {
        // <IDENT:
        jigds_arrpush(result->lineset, (jigVertexLineSet){ 0 });
        jigVertexLineSet* lines = &jigds_arrlast(result->lineset);
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, IDENT, UNKNOWN);
        lines->speaker = n.value;
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, COLON, UNKNOWN);

        // STRING,... >
        n = TNEXT(&ctx->tokens);
        while (n.type != JIG_TOKEN_GTH) {
            EH_CHECK(n, STRLIT, UNKNOWN);
            jigds_arrpush(lines->lines, n.value);
            n = TNEXT(&ctx->tokens);
            switch (n.type) {
            case JIG_TOKEN_COMMA:
                n = TNEXT(&ctx->tokens);
                break;
            case JIG_TOKEN_GTH:
                goto endlines;

            default:
                EH_FAIL(n, UNCLOSED_ANGLE);
            }
        }
endlines:
        n = TNEXT(&ctx->tokens);
    }

    switch (n.type) {
    case JIG_TOKEN_JOIN:
        // :: (EFFS) => IDENT
        result->type = JIG_VERTEX_JUMP;
        parse_effect(ctx, &result->v.jump.effects);
        EH_PROP();
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, ARROW, UNKNOWN);
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, IDENT, UNKNOWN);
        result->v.jump.next = n.value;
        jigds_arrpush(ctx->edges, n);
        break;

    case JIG_TOKEN_ARROW:
        // => IDENT
        result->type = JIG_VERTEX_JUMP;
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, IDENT, UNKNOWN);
        result->v.jump.next = n.value;
        jigds_arrpush(ctx->edges, n);
        break;
    case JIG_TOKEN_OPENPAREN:
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
static void parse_vert_if(jigParseCtx* ctx, jigVertexIfElse* result) {
    jigToken n = TNEXT(&ctx->tokens);
    while (n.type != JIG_TOKEN_EOF) {
        jigds_arrpush(result->blocks, (jigVertexIfElseBlock){ 0 });
        jigVertexIfElseBlock* block = &jigds_arrlast(result->blocks);

        // if (EXPR) then
        n = TPEEK(&ctx->tokens);
        EH_CHECK(n, OPENPAREN, UNKNOWN);
        jig_internal_parse_expr(ctx, &block->cond);
        EH_PROP();
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, KEYWORD, UNKNOWN);
        if (!STREQ(n.value, "then")) EH_FAIL(n, UNKNOWN);

        block->data = jig_allocator_allocate(ctx->alloc, sizeof(jigVertexBlock));
        *block->data = (jigVertexBlock){ 0 };
        parse_vert_if_body(ctx, block->data);
        EH_PROP();

        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, KEYWORD, UNKNOWN);
        if (STREQ(n.value, "else")) {
            if (STREQ(TPEEK(&ctx->tokens).value, "if")) {
                TNEXT(&ctx->tokens);
                continue;
            } else {
                result->catchall = jig_allocator_allocate(ctx->alloc, sizeof(jigVertexBlock));
                *result->catchall = (jigVertexBlock){ 0 };
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
static void parse_vert_if_body(jigParseCtx* ctx, jigVertexBlock* result) {
    jigToken n = TPEEK(&ctx->tokens);
    switch (n.type) {
    case JIG_TOKEN_KEYWORD:
        if (STREQ(n.value, "match")) {
            result->tag = JIG_VBLOCK_MATCH;
            parse_vert_match(ctx, &result->value.match);
        } else if (STREQ(n.value, "if")) {
            result->tag = JIG_VBLOCK_IFELSE;
            parse_vert_if(ctx, &result->value.ifelse);
        }
        break;
    case JIG_TOKEN_LTH:
        result->tag = JIG_VBLOCK_T;
        parse_vert(ctx, &result->value.vertex);
        break;

    default:
        EH_FAIL(n, BAD_VERTEX_BLOCK);
    }

    return;
}
static void parse_vert_match(jigParseCtx* ctx, jigVertexMatch* result) {
    jigToken n = TNEXT(&ctx->tokens);
    bool has_catchall = false;

    // match (EXPR) with
    n = TPEEK(&ctx->tokens);
    EH_CHECK(n, OPENPAREN, UNKNOWN);
    jig_internal_parse_expr(ctx, &result->expr);
    EH_PROP();
    n = TNEXT(&ctx->tokens);
    EH_CHECK(n, KEYWORD, UNKNOWN);
    if (!STREQ(n.value, "with")) EH_FAIL(n, UNKNOWN);

    n = TNEXT(&ctx->tokens);
    while (n.type != JIG_TOKEN_EOF) {
        switch (n.type) {
        case JIG_TOKEN_KEYWORD:
            if (!STREQ(n.value, "end")) EH_FAIL(n, UNCLOSED_CONDITIONAL);
            if (!has_catchall)          EH_FAIL(n, NO_CATCHALL);
            return;

        case JIG_TOKEN_OPENPAREN: {
            jigVertex* arm_vert;
            if (TPEEK(&ctx->tokens).type == JIG_TOKEN_CATCHALL) {
                TNEXT(&ctx->tokens);
                if (has_catchall) EH_FAIL(n, DUPLICATE_CASE);
                arm_vert = &result->catchall;
                has_catchall = true;
            } else {
                jigds_arrpush(result->arms, (jigVertexMatchArm){ 0 });
                jigVertexMatchArm* arm = &jigds_arrlast(result->arms);
                jig_internal_parse_value(ctx, &arm->cond);
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

static void parse_edge(jigParseCtx* ctx, jigEdge* result) {
    // STRING :: (EFFECT) => IDENT
    jigToken n = TNEXT(&ctx->tokens);
    EH_CHECK(n, STRLIT, UNKNOWN);
    result->line = n.value;
    n = TNEXT(&ctx->tokens);
    switch (n.type) {
    case JIG_TOKEN_JOIN:
        parse_effect(ctx, &result->effects);
        EH_PROP();
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, ARROW, UNKNOWN);
        break;

    case JIG_TOKEN_ARROW:
        break;

    default:
        EH_FAIL(n, UNKNOWN);
    }
    n = TNEXT(&ctx->tokens);
    EH_CHECK(n, IDENT, UNKNOWN);
    result->next = n.value;
    jigds_arrpush(ctx->edges, n);
}
static void parse_edge_if(jigParseCtx* ctx, jigEdgeIfElse* result) {
    jigToken n = TNEXT(&ctx->tokens);
    while (n.type != JIG_TOKEN_EOF) {
        jigds_arrpush(result->blocks, (jigEdgeIfElseBlock){ 0 });
        jigEdgeIfElseBlock* block = &jigds_arrlast(result->blocks);

        // if (EXPR) then
        n = TPEEK(&ctx->tokens);
        EH_CHECK(n, OPENPAREN, UNKNOWN);
        jig_internal_parse_expr(ctx, &block->cond);
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
static void parse_edge_if_body(jigParseCtx* ctx, jigEdgeBlock** result, bool top) {
    bool last_was_edge = false;
    jigToken n = TPEEK(&ctx->tokens);

    while (n.type != JIG_TOKEN_EOF) {
        n = TPEEK(&ctx->tokens);
        switch (n.type) {
        case JIG_TOKEN_CLOSEPAREN:
            if (top) {
                return;
            } else {
                EH_FAIL(n, UNKNOWN);
            }
            break;
        case JIG_TOKEN_KEYWORD:
            if (STREQ(n.value, "match")) {
                jigds_arrpush(*result, (jigEdgeBlock){ .tag=JIG_EBLOCK_MATCH });
                jigEdgeBlock* ref = &jigds_arrlast(*result);
                parse_edge_match(ctx, &ref->value.match);
            } else if (STREQ(n.value, "if")) {
                jigds_arrpush(*result, (jigEdgeBlock){ .tag=JIG_EBLOCK_IFELSE });
                jigEdgeBlock* ref = &jigds_arrlast(*result);
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
        case JIG_TOKEN_STRLIT:
            if (!last_was_edge) {
                jigds_arrpush(*result, (jigEdgeBlock){ .tag=JIG_EBLOCK_T });
            }
            jigEdgeBlock* ref = &jigds_arrlast(*result);
            jigds_arrpush(ref->value.edges, (jigEdge){ 0 });
            jigEdge* edge = &jigds_arrlast(ref->value.edges);
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
static void parse_edge_match(jigParseCtx* ctx, jigEdgeMatch* result) {
    jigToken n = TNEXT(&ctx->tokens);

    // match (EXPR) with
    n = TPEEK(&ctx->tokens);
    EH_CHECK(n, OPENPAREN, UNKNOWN);
    jig_internal_parse_expr(ctx, &result->expr);
    EH_PROP();
    n = TNEXT(&ctx->tokens);
    EH_CHECK(n, KEYWORD, UNKNOWN);
    if (!STREQ(n.value, "with")) EH_FAIL(n, UNKNOWN);

    n = TNEXT(&ctx->tokens);
    while (n.type != JIG_TOKEN_EOF) {
        switch (n.type) {
        case JIG_TOKEN_KEYWORD:
            if (!STREQ(n.value, "end")) EH_FAIL(n, UNCLOSED_CONDITIONAL);
            return;

        case JIG_TOKEN_OPENPAREN: {
            jigEdge* arm_es;
            if (TPEEK(&ctx->tokens).type == JIG_TOKEN_CATCHALL) {
                if (result->catchall.exists) EH_FAIL(n, DUPLICATE_CASE);
                TNEXT(&ctx->tokens);
                result->catchall.exists = true;
                arm_es = &result->catchall.arm;
            } else {
                jigds_arrpush(result->arms, (jigEdgeMatchArm){ 0 });
                jigEdgeMatchArm* arm = &jigds_arrlast(result->arms);
                jig_internal_parse_value(ctx, &arm->cond);
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
static void parse_effect(jigParseCtx* ctx, jigEffect** result) {
    jigToken n = TNEXT(&ctx->tokens);
    EH_CHECK(n, OPENPAREN, UNKNOWN);

    n = TNEXT(&ctx->tokens);
    while (n.type != JIG_TOKEN_CLOSEPAREN) {
        EH_CHECK(n, IDENT, INVALID_ASSIGN);
        jigds_arrpush(*result, ((jigEffect){ .lhs=n.value, .op=JIG_EFFECT_EVENT }));
        jigEffect* effect = &jigds_arrlast(*result);

        n = TNEXT(&ctx->tokens);
        switch (n.type) {
        case JIG_TOKEN_ADDEQ: case JIG_TOKEN_SUBEQ: case JIG_TOKEN_MULEQ: case JIG_TOKEN_DIVEQ: case JIG_TOKEN_SETEQ:
            switch (n.type) {
            case JIG_TOKEN_ADDEQ:
                effect->op = JIG_EFFECT_ADD; break;
            case JIG_TOKEN_SUBEQ:
                effect->op = JIG_EFFECT_SUB; break;
            case JIG_TOKEN_MULEQ:
                effect->op = JIG_EFFECT_MUL; break;
            case JIG_TOKEN_DIVEQ:
                effect->op = JIG_EFFECT_DIV; break;
            case JIG_TOKEN_SETEQ:
                effect->op = JIG_EFFECT_SET; break;
            default:
                EH_FAIL(n, UNREACHABLE);
                break;
            }
            jig_internal_parse_expr(ctx, &effect->rhs);
            EH_PROP();
            n = TNEXT(&ctx->tokens);
            switch (n.type) {
            case JIG_TOKEN_COMMA:
                n = TNEXT(&ctx->tokens);
                break;
            case JIG_TOKEN_CLOSEPAREN:
                break;

            default:
                EH_FAIL(n, UNCLOSED_PAREN);
            }
            break;

        case JIG_TOKEN_COMMA:
            n = TNEXT(&ctx->tokens);
            break;
        case JIG_TOKEN_CLOSEPAREN:
            return;

        default:
            EH_FAIL(n, INVALID_OPERATOR);
        }
    }
}

