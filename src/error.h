#ifndef CQUOSI_IMPL_ERROR_H
#define CQUOSI_IMPL_ERROR_H
#include "lex.h"

int quosi_internal_error_handle(quosiError* errs, quosiToken tok, int type);
int quosi_internal_error_check(quosiError* errs, quosiToken tok, int expect, int failtype);


// #define EH_FAIL(tok, E)     quosi_internal_error_handle(ctx->errors, tok, QUOSI_ERR_##E)
// #define EH_CHECK(tok, T, E) quosi_internal_error_check(ctx->errors, tok, QUOSI_TOKEN_##T, QUOSI_ERR_##E)
// #define EH_PROP()           (ctx->errors->critical)

#define EH_FAIL(tok, E)     do { if (quosi_internal_error_handle(ctx->errors, tok, QUOSI_ERR_##E)) return; } while (0)
#define EH_CHECK(tok, T, E) do { if (quosi_internal_error_check(ctx->errors, tok, QUOSI_TOKEN_##T, QUOSI_ERR_##E)) return; } while (0)
#define EH_PROP()           do { if (ctx->errors->critical) return; } while (0)

#define EH_FAIL_RET(tok, E)     do { if (quosi_internal_error_handle(ctx->errors, tok, QUOSI_ERR_##E)) return result; } while (0)
#define EH_CHECK_RET(tok, T, E) do { if (quosi_internal_error_check(ctx->errors, tok, QUOSI_TOKEN_##T, QUOSI_ERR_##E)) return result; } while (0)
#define EH_PROP_RET()           do { if (ctx->errors->critical) return result; } while (0)


#endif
