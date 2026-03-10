
#if 0

#define _CRT_SECURE_NO_WARNINGS
#define _GNU_SOURCE
#define VANGO_BENCH_WARMUP 1000
#include <vangotest/casserts2.h>
#include <vangotest/bench.h>
#include "jig/jig.h"
#include "jig/ast.h"
#include <stdlib.h>
#include "fsutil.h"


static uint32_t dummy_ctxf(const char* key, void* user_data) { (void)key; (void)user_data; return 0; }
static jigSymbolCtx dummy_ctx = { dummy_ctxf, dummy_ctxf, NULL, NULL };

vango_test(bench_memory) {
    char* src = read_to_string("examples/large.jig");
    vg_assert_non_null(src);
    jigError errors = { 0 };
    jigMemoryArena arena = jig_memory_arena_create(JIG_MEMORY_ARENA_PAGE, 200 * 1000);
    jigAst ast;

    vango_bench(10000, {
        jig_memory_arena_reset(&arena);
        ast = jig_ast_parse_from_src(src, &errors, jig_memory_arena_allocator(&arena));
    });
    vango_bench(10000, {
        ast = jig_ast_parse_from_src(src, &errors, jig_malloc_allocator());
        jig_ast_free(&ast, jig_malloc_allocator());
    });

    jig_memory_arena_destroy(&arena);
    if (errors.list) {
        jig_error_list_free(&errors);
        vg_assert(false);
    }
    free(src);
}

vango_test(bench_parser) {
    char* src = read_to_string("examples/large.jig");
    vg_assert_non_null(src);
    jigError errors = { 0, 0 };
    jigMemoryArena arena = jig_memory_arena_create(JIG_MEMORY_ARENA_PAGE, 100 * 1000);
    jigAst ast;

    ast = jig_ast_parse_from_src(src, &errors, jig_malloc_allocator());
    jig_ast_free(&ast, jig_malloc_allocator());

    vango_bench(10000, {
        jig_memory_arena_reset(&arena);
        ast = jig_ast_parse_from_src(src, &errors, jig_memory_arena_allocator(&arena));
    });

    vg_assert_null(errors.list);
    jig_memory_arena_destroy(&arena);
    free(src);
}

vango_test(bench_codegen) {
    char* src = read_to_string("examples/large.jig");
    jigError errors = { 0 };
    jigMemoryArena ast_arena = jig_memory_arena_create(JIG_MEMORY_ARENA_PAGE, 100 * 1000);
    const jigAst ast = jig_ast_parse_from_src(src, &errors, jig_memory_arena_allocator(&ast_arena));
    jigFile* file = NULL;

    jigMemoryArena cmp_arena = jig_memory_arena_create(JIG_MEMORY_ARENA_PAGE, 100 * 1000);

    vango_bench(10000, {
        jig_memory_arena_reset(&cmp_arena);
        file = jig_file_compile_from_ast(&ast, dummy_ctx, jig_memory_arena_allocator(&cmp_arena));
    });

    vg_assert_non_null(file);
    jig_memory_arena_destroy(&ast_arena);
    jig_memory_arena_destroy(&cmp_arena);
    free(src);
}

vango_test(bench_compiler) {
    char* src = read_to_string("examples/large.jig");
    jigError errors = { 0 };
    jigMemoryArena arena = jig_memory_arena_create(JIG_MEMORY_ARENA_MALLOC, 100 * 1000);
    jigFile* file = NULL;

    vango_bench(10000, {
        jig_memory_arena_reset(&arena);
        file = jig_file_compile_from_src(src, &errors, dummy_ctx, jig_memory_arena_allocator(&arena));
    });

    vg_assert_non_null(file);
    jig_memory_arena_destroy(&arena);
    free(src);
}

#else

void _filler(void) {}

#endif
