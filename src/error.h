#ifndef CJIG_IMPL_ERROR_H
#define CJIG_IMPL_ERROR_H
#include "lex.h"

int jig_internal_error_handle(jigError* errs, jigToken tok, int type);
int jig_internal_error_check(jigError* errs, jigToken tok, int expect, int failtype);


// #define EH_FAIL(tok, E)     jig_internal_error_handle(ctx->errors, tok, JIG_ERR_##E)
// #define EH_CHECK(tok, T, E) jig_internal_error_check(ctx->errors, tok, JIG_TOKEN_##T, JIG_ERR_##E)
// #define EH_PROP()           (ctx->errors->critical)

#define EH_FAIL(tok, E)     do { if (jig_internal_error_handle(ctx->errors, tok, JIG_ERR_##E)) return; } while (0)
#define EH_CHECK(tok, T, E) do { if (jig_internal_error_check(ctx->errors, tok, JIG_TOKEN_##T, JIG_ERR_##E)) return; } while (0)
#define EH_PROP()           do { if (ctx->errors->critical) return; } while (0)

#define EH_FAIL_RET(tok, E)     do { if (jig_internal_error_handle(ctx->errors, tok, JIG_ERR_##E)) return result; } while (0)
#define EH_CHECK_RET(tok, T, E) do { if (jig_internal_error_check(ctx->errors, tok, JIG_TOKEN_##T, JIG_ERR_##E)) return result; } while (0)
#define EH_PROP_RET()           do { if (ctx->errors->critical) return result; } while (0)


#endif
