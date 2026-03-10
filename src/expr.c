#include "jig/jig.h"
#include "jig/ast.h"
#include "jig/bc.h"
#include "ctx.h"
#include "lex.h"
#include "error.h"
#include <string.h>
#include <stdio.h>
#include <math.h>


struct Assoc { float lhs; float rhs; };

static struct Assoc binding(jigToken t) {
    switch (t.type) {
    case JIG_TOKEN_LOGOR:
        return (struct Assoc){ 1.1f, 1.f };
    case JIG_TOKEN_LOGAND:
        return (struct Assoc){ 2.1f, 2.f };
    case JIG_TOKEN_EQU: case JIG_TOKEN_NEQ:
        return (struct Assoc){ 3.1f, 3.f };
    case JIG_TOKEN_LTH: case JIG_TOKEN_GTH: case JIG_TOKEN_LEQ: case JIG_TOKEN_GEQ:
        return (struct Assoc){ 4.1f, 4.f };
    case JIG_TOKEN_ADD: case JIG_TOKEN_SUB:
        return (struct Assoc){ 5.1f, 5.f };
    case JIG_TOKEN_MUL: case JIG_TOKEN_DIV:
        return (struct Assoc){ 6.1f, 6.f };
    case JIG_TOKEN_COLON:
        return (struct Assoc){ 7.1f, 7.f };
    case JIG_TOKEN_LOGNOT:
        return (struct Assoc){ 0.f, 8.f };

    default:
        return (struct Assoc){ -INFINITY, -INFINITY };
    }
}
static uint8_t opcode(jigToken t) {
    switch (t.type) {
    case JIG_TOKEN_LOGOR:  return JIG_INSTR_LOR;
    case JIG_TOKEN_LOGAND: return JIG_INSTR_LAND;
    case JIG_TOKEN_LOGNOT: return JIG_INSTR_LNOT;
    case JIG_TOKEN_EQU:    return JIG_INSTR_EQU;
    case JIG_TOKEN_NEQ:    return JIG_INSTR_NEQ;
    case JIG_TOKEN_LTH:    return JIG_INSTR_LTH;
    case JIG_TOKEN_GTH:    return JIG_INSTR_GTH;
    case JIG_TOKEN_LEQ:    return JIG_INSTR_LEQ;
    case JIG_TOKEN_GEQ:    return JIG_INSTR_GEQ;
    case JIG_TOKEN_ADD:    return JIG_INSTR_ADD;
    case JIG_TOKEN_SUB:    return JIG_INSTR_SUB;
    case JIG_TOKEN_MUL:    return JIG_INSTR_MUL;
    case JIG_TOKEN_DIV:    return JIG_INSTR_DIV;
    case JIG_TOKEN_COLON:  return JIG_INSTR_STORE;
    default: return JIG_INSTR_EOF;
    }
}


#define TNEXT(stream) jig_token_stream_next(stream)
#define TPEEK(stream) jig_token_stream_peek(stream)
#define STREQ(view, cptr) ((view.len == sizeof(cptr)-1) && (strncmp(view.ptr, cptr, view.len) == 0))


static void parse_expr_impl(jigParseCtx* ctx, jigExpr* result, float minbp, int l);

void jig_internal_parse_expr(jigParseCtx* ctx, jigExpr* result) {
    parse_expr_impl(ctx, result, 0.f, 0);
}
void jig_internal_parse_value(jigParseCtx* ctx, jigExpr* result) {
    jigToken n = TNEXT(&ctx->tokens);
    switch (n.type) {
    case JIG_TOKEN_IDENT:
        result->value.ident = n.value;
        result->tag = JIG_EXPR_IDENT;
        break;

    case JIG_TOKEN_KEYWORD:
        if (STREQ(n.value, "true")) {
            result->value.imm = (uint64_t)1;
            result->tag = JIG_EXPR_IMM;
        } else if (STREQ(n.value, "false")) {
            result->value.imm = (uint64_t)0;
            result->tag = JIG_EXPR_IMM;
        } else {
            EH_FAIL(n, INVALID_ATOM);
        }
        break;

    case JIG_TOKEN_NUMBER:
        result->value.imm = 0;
        for (size_t i = 0; i < n.value.len; i++) {
            result->value.imm = (10 * result->value.imm) + (uint64_t)(n.value.ptr[i] - '0');
        }
        result->tag = JIG_EXPR_IMM;
        break;

    default:
        EH_FAIL(n, INVALID_ATOM);
        break;
    }
    return;
}

static void parse_expr_impl(jigParseCtx* ctx, jigExpr* result, float minbp, int l) {
    jigToken n = TPEEK(&ctx->tokens);
    switch (n.type) {
    case JIG_TOKEN_OPENPAREN:
        TNEXT(&ctx->tokens);
        parse_expr_impl(ctx, result, 0, l + 1);
        EH_PROP();
        n = TNEXT(&ctx->tokens);
        EH_CHECK(n, CLOSEPAREN, UNCLOSED_PAREN);
        if (l == 0) return;
        break;

    case JIG_TOKEN_LOGNOT: {
        TNEXT(&ctx->tokens);
        result->lhs = jig_allocator_allocate(ctx->alloc, sizeof(jigExpr));
        parse_expr_impl(ctx, result->lhs, 0, l);
        EH_PROP();
        result->value.op = JIG_INSTR_LNOT;
        result->tag = JIG_EXPR_OP;
        break; }

    case JIG_TOKEN_IDENT: case JIG_TOKEN_KEYWORD: case JIG_TOKEN_NUMBER:
        jig_internal_parse_value(ctx, result);
        EH_PROP();
        break;

    default:
        EH_FAIL(n, INVALID_ATOM);
        break;
    }

    while (true) {
        const jigToken op = TPEEK(&ctx->tokens);
        if (op.type == JIG_TOKEN_COMMA) break;
        if (op.type == JIG_TOKEN_CLOSEPAREN) break;
        const struct Assoc bindstr = binding(op);
        if (bindstr.lhs < 0.f) EH_FAIL(op, INVALID_OPERATOR);
        if (bindstr.lhs < minbp) break;
        TNEXT(&ctx->tokens);
        jigExpr* lhs = jig_allocator_allocate(ctx->alloc, sizeof(jigExpr));
        *lhs = *result;
        result->lhs = lhs;
        result->rhs = jig_allocator_allocate(ctx->alloc, sizeof(jigExpr));
        parse_expr_impl(ctx, result->rhs, bindstr.rhs, l);
        EH_PROP();
        result->value.op = opcode(op);
        result->tag = JIG_EXPR_OP;
        if (result->value.op == JIG_INSTR_STORE && result->lhs->tag != JIG_EXPR_IDENT) {
            EH_FAIL(op, INVALID_ASSIGN);
        }
        if (TPEEK(&ctx->tokens).type == JIG_TOKEN_COMMA) break;
    }

    return;
}

