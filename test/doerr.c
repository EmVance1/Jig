#define _CRT_SECURE_NO_WARNINGS
#define _GNU_SOURCE
#include <vangotest/casserts2.h>
#include "jig/jig.h"
#include <stdlib.h>
// #include <stdio.h>


static uint32_t dummy_ctxf(const char* key, void* user_data) { (void)key; (void)user_data; return 0; }
static jigSymbolCtx dummy_ctx = { dummy_ctxf, dummy_ctxf, NULL, NULL };

static void expect_single_fail(VANGO_TEST_PARAMS, int err, const char* src) {
    jigError errors = { 0 };
    free(jig_file_compile_from_src(src, &errors, dummy_ctx, jig_malloc_allocator()));
    vg_assert_non_null(errors.list);
    vg_assert_eq(err, errors.list[0].type);
    // printf("jig compile error (%u:%u): %s\n", errors.list[0].span.row, errors.list[0].span.col, jig_error_to_string(errors.list[0]));
    jig_error_list_free(&errors);
}


vango_test(early_eof) {
    expect_single_fail(_vango_test_result, JIG_ERR_EARLY_EOF,
        "module Eof START = <Brian: \"Hello there.\"> => EXIT");
}

vango_test(invalid_token) {
    expect_single_fail(_vango_test_result, JIG_ERR_INVALID_TOKEN,
        "module Token START & <Brian: \"Hello there.\"> => EXIT endmod");
}

vango_test(invalid_atom) {
    expect_single_fail(_vango_test_result, JIG_ERR_INVALID_ATOM,
        "module Atom START = <Brian: \"Hello there.\"> :: (a += =) => EXIT endmod");
}

vango_test(invalid_operator1) {
    expect_single_fail(_vango_test_result, JIG_ERR_INVALID_OPERATOR,
        "module Op START = <Brian: \"Hello there.\"> :: (a / 5) => EXIT endmod");
}

vango_test(invalid_operator2) {
    expect_single_fail(_vango_test_result, JIG_ERR_INVALID_OPERATOR,
        "module Op START = if (a ? 25) then <Brian: \"Hello there.\"> => EXIT else <Brian: \"Byebye.\"> => EXIT end endmod");
}

vango_test(invalid_assign1) {
    expect_single_fail(_vango_test_result, JIG_ERR_INVALID_ASSIGN,
        "module Ass START = <Brian: \"Hello there.\"> :: (0 += 5) => EXIT endmod");
}

vango_test(invalid_assign2) {
    expect_single_fail(_vango_test_result, JIG_ERR_INVALID_ASSIGN,
        "module Ass START = if (0:25) then <Brian: \"Hello there.\"> => EXIT else <Brian: \"Byebye.\"> => EXIT end endmod");
}

