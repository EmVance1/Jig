#ifndef CJIG_INTERPRETER_H
#define CJIG_INTERPRETER_H
#include "jig/jig.h"
#include <stdint.h>


enum jigUpcall {
    JIG_UPCALL_NONE = 0,
    JIG_UPCALL_LINE,
    JIG_UPCALL_PICK,
    JIG_UPCALL_EVENT,
    JIG_UPCALL_EXIT,
    JIG_UPCALL_ABORT,
};
typedef uint64_t*(*jigVmCtx)(uint32_t key, void* user_data);
typedef struct jigProposition {
    const char* str;
    uint8_t idx;
} jigProposition;

#ifndef JIG_PROP_QUEUE_SIZE
#define JIG_PROP_QUEUE_SIZE 16
#endif

#ifndef JIG_VALUE_STACK_SIZE
#define JIG_VALUE_STACK_SIZE 128
#endif

#define JIG_VERTEX_START 0
#define JIG_VERTEX_EXIT  UINT32_MAX

typedef struct jigVm {
    jigProposition text[JIG_PROP_QUEUE_SIZE];
    uint64_t stack[JIG_VALUE_STACK_SIZE];
    uint32_t PC, SP;
    uint32_t TH, TT;
    uint32_t A,  B;
    const uint8_t* base;
    const uint8_t* code;
    const uint8_t* strs;
} jigVm;

void jig_vm_init(jigVm* self, const jigFile* file, const char* module);

const char* jig_vm_line(const jigVm* self);
uint32_t    jig_vm_id(const jigVm* self);
uint32_t    jig_vm_nq(const jigVm* self);

void             jig_vm_push_value(jigVm* self, uint64_t val);
uint64_t         jig_vm_pop_value(jigVm* self);
uint64_t         jig_vm_top_value(const jigVm* self);
jigProposition jig_vm_dequeue_text(jigVm* self);

int jig_vm_exec(jigVm* self, jigVmCtx ctx, void* user_data);


#endif
