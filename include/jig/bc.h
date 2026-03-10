#ifndef CJIG_BC_H
#define CJIG_BC_H
#include "jig/jig.h"
#include <stdint.h>


enum jigInstr {
    JIG_INSTR_EOF = 0,

    JIG_INSTR_PUSH,
    JIG_INSTR_POP,
    JIG_INSTR_DUP,
    JIG_INSTR_LOAD,
    JIG_INSTR_STORE,

    JIG_INSTR_LAND,
    JIG_INSTR_LOR,
    JIG_INSTR_LNOT,
    JIG_INSTR_ADD,
    JIG_INSTR_SUB,
    JIG_INSTR_MUL,
    JIG_INSTR_DIV,
    JIG_INSTR_NEG,
    JIG_INSTR_EQU,
    JIG_INSTR_NEQ,
    JIG_INSTR_IEQV,
    JIG_INSTR_IEQK,
    JIG_INSTR_LEQ,
    JIG_INSTR_LTH,
    JIG_INSTR_GEQ,
    JIG_INSTR_GTH,

    JIG_INSTR_JUMP,
    JIG_INSTR_JZ,
    JIG_INSTR_JNZ,
    JIG_INSTR_SWITCH,

    JIG_INSTR_PROP,
    JIG_INSTR_PICK,
    JIG_INSTR_LINE,
    JIG_INSTR_EVENT,
};

typedef struct jigModData {
    jigStrView name;
    uint32_t entry;
    // vector
    uint8_t* code;
} jigModData;

typedef struct jigProgramData {
    // vector
    jigModData* mods;
    // vector
    uint8_t* strs;
    // vector
    uint8_t* syms;
} jigProgramData;

jigProgramData jig_compile_ast(const struct jigAst* ast, jigSymbolCtx ctx, jigAllocator alloc);
void jig_program_data_free(jigProgramData* data, jigAllocator alloc);


#endif
