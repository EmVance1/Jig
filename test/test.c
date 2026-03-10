#define _CRT_SECURE_NO_WARNINGS
#define _GNU_SOURCE
// #define VANGO_TEST_MEMORY_LEAKS
#define VANGO_BENCH_WARMUP 1000
#define VANGO_TEST_ROOT
#include <vangotest/casserts2.h>
#include <vangotest/bench.h>
#include "jig/jig.h"
#include "jig/vm.h"
#include <stdlib.h>
#include <stdio.h>
#include "fsutil.h"


static uint32_t dummy_ctxf(const char* key, void* user_data) { (void)key; (void)user_data; return 0; }
static jigSymbolCtx dummy_ctx = { dummy_ctxf, dummy_ctxf, NULL, NULL };
static uint64_t* vm_ctx(uint32_t key, void* user_data) { (void)key; (void)user_data; static uint64_t val = 0; return &val; }


vango_test(cmp_example) {
    char* src = read_to_string("examples/brian.jig");
    vg_assert_non_null(src);
    jigError errors = { 0 };
    jigFile* file = jig_file_compile_from_src(src, &errors, dummy_ctx, jig_malloc_allocator());
    free(src);

    if (errors.list != NULL) {
        for (size_t i = 0; i < jig_error_list_len(&errors); i++) {
            printf("jig compile error (%u:%u): %s\n", errors.list[i].span.row, errors.list[i].span.col, jig_error_to_string(errors.list[i]));
        }
        jig_error_list_free(&errors);
        vg_assert(false);
    }

    // jig_file_prettyprint(file, "Default", stdout);
    free(file);
}

vango_test(free_on_err) {
    char* src = "module Op START = if (a ? 25) then <Brian: \"Hello there.\"> => EXIT else <Brian: \"Byebye.\"> => EXIT end endmod";
    jigError errors = { 0 };
    jigFile* file = NULL;

    vango_bench(10000, {
        free(file);
        file = jig_file_compile_from_src(src, &errors, dummy_ctx, jig_malloc_allocator());
        jig_error_list_free(&errors);
    });

    vg_assert_null(file);
    free(file);
}


/*
vango_test(run_example) {
    char* src = read_to_string("examples/doall.jig");
    vg_assert_non_null(src);
    jigError errors = { 0 };
    jigFile* file = jig_file_compile_from_src(src, &errors, dummy_ctx, jig_malloc_allocator());
    free(src);

    jigVm vm;
    jig_vm_init(&vm, file, "Default");

    while (true) {
        switch (jig_vm_exec(&vm, vm_ctx)) {
        case JIG_UPCALL_LINE:
            printf("%u: \"%s\"\n", jig_vm_id(&vm), jig_vm_line(&vm));
            break;
        case JIG_UPCALL_PICK: {
            uint32_t pindex[JIG_PROP_QUEUE_SIZE];
            for (uint32_t i = 0; i < jig_vm_nq(&vm); i++) {
                const jigProposition prop = jig_vm_dequeue_text(&vm);
                printf("  %u: \"%s\"\n", i+1, prop.str);
                pindex[i] = prop.idx;
            }
            printf("\n> ");
            uint32_t pick;
            scanf("%u", &pick);
            printf("\n");
            jig_vm_push_value(&vm, pindex[pick-1]);
            break; }
        case JIG_UPCALL_EVENT:
            printf("EVENT: %s\n\n", jig_vm_line(&vm));
            break;
        case JIG_UPCALL_EXIT:
            printf("\nEOF\n");
            free(file);
            return;
        default:
            printf("\nERROR\n");
            free(file);
            vg_assert(false);
        }
    }
}
*/

