#ifndef CJIG_IMPL_EXPR_H
#define CJIG_IMPL_EXPR_H
#include "jig/ast.h"
#include "ctx.h"

void jig_internal_parse_expr(jigParseCtx* ctx, jigExpr* result);
void jig_internal_parse_value(jigParseCtx* ctx, jigExpr* result);

#endif
