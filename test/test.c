#define _CRT_SECURE_NO_WARNINGS
#define _GNU_SOURCE
// #define VANGO_TEST_MEMORY_LEAKS
#define VANGO_BENCH_WARMUP 1000
#define VANGO_TEST_ROOT
#include <vangotest/casserts2.h>
#include <vangotest/bench.h>
#include "quosi/quosi.h"
#include "quosi/vm.h"
#include <stdlib.h>
#include <stdio.h>
#include "fsutil.h"


static uint32_t dummy_ctxf(const char* key) { (void)key; return 0; }
static quosiSymbolCtx dummy_ctx = { dummy_ctxf, dummy_ctxf };
static uint64_t* vm_ctx(uint32_t key) { (void)key; static uint64_t val = 0; return &val; }


vango_test(cmp_example) {
    char* src = read_to_string("examples/brian.qsi");
    vg_assert_non_null(src);
    quosiError errors = { 0 };
    quosiFile* file = quosi_file_compile_from_src(src, &errors, dummy_ctx, quosi_malloc_allocator());
    free(src);

    if (errors.list != NULL) {
        for (size_t i = 0; i < quosi_error_list_len(&errors); i++) {
            printf("quosi compile error (%u:%u): %s\n", errors.list[i].span.row, errors.list[i].span.col, quosi_error_to_string(errors.list[i]));
        }
        quosi_error_list_free(&errors);
        vg_assert(false);
    }

    // quosi_file_prettyprint(file, "Default", stdout);
    free(file);
}

vango_test(free_on_err) {
    char* src = "module Op START = if (a ? 25) then <Brian: \"Hello there.\"> => EXIT else <Brian: \"Byebye.\"> => EXIT end endmod";
    quosiError errors = { 0 };
    quosiFile* file = NULL;

    vango_bench(10000, {
        free(file);
        file = quosi_file_compile_from_src(src, &errors, dummy_ctx, quosi_malloc_allocator());
        quosi_error_list_free(&errors);
    });

    vg_assert_null(file);
    free(file);
}


/*
vango_test(run_example) {
    char* src = read_to_string("examples/doall.qsi");
    vg_assert_non_null(src);
    quosiError errors = { 0 };
    quosiFile* file = quosi_file_compile_from_src(src, &errors, dummy_ctx, quosi_malloc_allocator());
    free(src);

    quosiVm vm;
    quosi_vm_init(&vm, file, "Default");

    while (true) {
        switch (quosi_vm_exec(&vm, vm_ctx)) {
        case QUOSI_UPCALL_LINE:
            printf("%u: \"%s\"\n", quosi_vm_id(&vm), quosi_vm_line(&vm));
            break;
        case QUOSI_UPCALL_PICK: {
            uint32_t pindex[QUOSI_PROP_QUEUE_SIZE];
            for (uint32_t i = 0; i < quosi_vm_nq(&vm); i++) {
                const quosiProposition prop = quosi_vm_dequeue_text(&vm);
                printf("  %u: \"%s\"\n", i+1, prop.str);
                pindex[i] = prop.idx;
            }
            printf("\n> ");
            uint32_t pick;
            scanf("%u", &pick);
            printf("\n");
            quosi_vm_push_value(&vm, pindex[pick-1]);
            break; }
        case QUOSI_UPCALL_EVENT:
            printf("EVENT: %s\n\n", quosi_vm_line(&vm));
            break;
        case QUOSI_UPCALL_EXIT:
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

