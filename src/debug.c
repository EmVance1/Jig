#include "jig/jig.h"
#include "jig/bc.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>


static bool jumps_contains(const uint32_t* jumps, size_t njs, uint32_t target) {
    if (target == UINT32_MAX) return true;
    for (size_t i = 0; i < njs; i++) {
        if (jumps[i] == target) {
            return true;
        }
    }
    return false;
}
static uint32_t jumps_get(const uint32_t* jumps, size_t njs, uint32_t target) {
    if (target == UINT32_MAX) return 999999;
    for (size_t i = 0; i < njs; i++) {
        if (jumps[i] == target) {
            return (uint32_t)i;
        }
    }
    return 0;
}

void jig_file_prettyprint(const jigFile* bin, const char* module, void* _file) {
    const jigFileModTableEntry mod = jig_file_module(bin, module);
    const uint8_t* code    = mod.code;
    const size_t code_len  = mod.len;
    const size_t entry_pos = mod.entry;
    const uint8_t* strs = jig_file_strs(bin);
    FILE* f = (FILE*)_file;

    uint32_t PC = 0;
    uint8_t  a1 = 0;
    uint32_t a2 = 0;
    uint64_t a3 = 0;
    uint32_t skip = 0;

    uint32_t jumps[512] = { 0 };
    size_t njs = 0;

    while (PC < code_len) {
        switch (code[PC++]) {
        case JIG_INSTR_JUMP:
            memcpy(&a2, code + PC, sizeof(uint32_t));
            if (!jumps_contains(jumps, njs, a2)) jumps[njs++] = a2;
            PC += sizeof(uint32_t);
            break;
        case JIG_INSTR_JZ:
            memcpy(&a2, code + PC, sizeof(uint32_t));
            if (!jumps_contains(jumps, njs, a2)) jumps[njs++] = a2;
            PC += sizeof(uint32_t);
            break;
        case JIG_INSTR_JNZ:
            memcpy(&a2, code + PC, sizeof(uint32_t));
            if (!jumps_contains(jumps, njs, a2)) jumps[njs++] = a2;
            PC += sizeof(uint32_t);
            break;
        case JIG_INSTR_SWITCH:
            for (uint32_t i = 0; i < skip; i++) {
                memcpy(&a2, code + PC, sizeof(uint32_t));
                if (!jumps_contains(jumps, njs, a2)) jumps[njs++] = a2;
                PC += sizeof(uint32_t);
            }
            skip = 0;
            break;
        case JIG_INSTR_PROP:
            PC += sizeof(uint32_t) + sizeof(uint8_t);
            skip++;
            break;
        default:
            break;
        }
    }
    PC = 0;
    skip = 0;
    while (PC < code_len) {
        if (jumps_contains(jumps, njs, PC)) {
            fprintf(f, "    .L%d:\n", jumps_get(jumps, njs, PC));
        } else if (PC == entry_pos) {
            fprintf(f, "    <START>:\n");
        }
        switch (code[PC++]) {
        case JIG_INSTR_EOF:
            return;

        case JIG_INSTR_PUSH:
            memcpy(&a3, code + PC, sizeof(uint64_t));
            fprintf(f, "0x%04X    PUSH $%" PRIu64 "\n", PC-1, a3);
            PC += sizeof(uint64_t);
            break;
        case JIG_INSTR_POP:
            fprintf(f, "0x%04X    POP\n", PC-1);
            break;
        case JIG_INSTR_DUP:
            fprintf(f, "0x%04X    DUP\n", PC-1);
            break;

        case JIG_INSTR_LOAD:
            memcpy(&a2, code + PC, sizeof(uint32_t));
            fprintf(f, "0x%04X    LOAD @%u\n", PC-1, a2);
            PC += sizeof(uint32_t);
            break;
        case JIG_INSTR_STORE:
            memcpy(&a2, code + PC, sizeof(uint32_t));
            fprintf(f, "0x%04X    STORE @%u\n", PC-1, a2);
            PC += sizeof(uint32_t);
            break;

        case JIG_INSTR_LAND:
            fprintf(f, "0x%04X    LAND\n", PC-1);
            break;
        case JIG_INSTR_LOR:
            fprintf(f, "0x%04X    LOR\n", PC-1);
            break;
        case JIG_INSTR_LNOT:
            fprintf(f, "0x%04X    LNOT\n", PC-1);
            break;
        case JIG_INSTR_ADD:
            fprintf(f, "0x%04X    ADD\n", PC-1);
            break;
        case JIG_INSTR_SUB:
            fprintf(f, "0x%04X    SUB\n", PC-1);
            break;
        case JIG_INSTR_MUL:
            fprintf(f, "0x%04X    MUL\n", PC-1);
            break;
        case JIG_INSTR_DIV:
            fprintf(f, "0x%04X    DIV\n", PC-1);
            break;
        case JIG_INSTR_NEG:
            fprintf(f, "0x%04X    NEG\n", PC-1);
            break;
        case JIG_INSTR_EQU:
            fprintf(f, "0x%04X    EQU\n", PC-1);
            break;
        case JIG_INSTR_NEQ:
            fprintf(f, "0x%04X    NEQ\n", PC-1);
            break;
        case JIG_INSTR_IEQV:
            memcpy(&a3, code + PC, sizeof(uint64_t));
            fprintf(f, "0x%04X    IEQ  $%" PRIu64 "\n", PC-1, a3);
            PC += sizeof(uint64_t);
            break;
        case JIG_INSTR_IEQK:
            memcpy(&a2, code + PC, sizeof(uint32_t));
            fprintf(f, "0x%04X    IEQ  %u\n", PC-1, a2);
            PC += sizeof(uint32_t);
            break;
        case JIG_INSTR_LEQ:
            fprintf(f, "0x%04X    LEQ\n", PC-1);
            break;
        case JIG_INSTR_LTH:
            fprintf(f, "0x%04X    LTH\n", PC-1);
            break;
        case JIG_INSTR_GEQ:
            fprintf(f, "0x%04X    GEQ\n", PC-1);
            break;
        case JIG_INSTR_GTH:
            fprintf(f, "0x%04X    GTH\n", PC-1);
            break;
        case JIG_INSTR_JUMP:
            memcpy(&a2, code + PC, sizeof(uint32_t));
            fprintf(f, "0x%04X    JUMP .L%u\n", PC-1, jumps_get(jumps, njs, a2));
            PC += sizeof(uint32_t);
            break;
        case JIG_INSTR_JZ:
            memcpy(&a2, code + PC, sizeof(uint32_t));
            fprintf(f, "0x%04X    JZ   .L%u\n", PC-1, jumps_get(jumps, njs, a2));
            PC += sizeof(uint32_t);
            break;
        case JIG_INSTR_JNZ:
            memcpy(&a2, code + PC, sizeof(uint32_t));
            fprintf(f, "0x%04X    JNZ  .L%u\n", PC-1, jumps_get(jumps, njs, a2));
            PC += sizeof(uint32_t);
            break;
        case JIG_INSTR_SWITCH:
            fprintf(f, "0x%04X    SWITCH [ ", PC-1);
            for (uint32_t i = 0; i < skip; i++) {
                memcpy(&a2, code + PC, sizeof(uint32_t));
                fprintf(f, ".L%u", jumps_get(jumps, njs, a2));
                if (i < skip - 1) fprintf(f, ", ");
                PC += sizeof(uint32_t);
            }
            fprintf(f, " ]\n");
            skip = 0;
            break;

        case JIG_INSTR_PROP:
            fprintf(f, "0x%04X    ", PC-1);
            memcpy(&a2, code + PC, sizeof(uint32_t));
            PC += sizeof(uint32_t);
            memcpy(&a1, code + PC, sizeof(uint8_t));
            PC += sizeof(uint8_t);
            fprintf(f, "PROP \"%s\", %d\n", (const char*)strs + a2, (int)a1);
            skip++;
            break;
        case JIG_INSTR_LINE:
            fprintf(f, "0x%04X    ", PC-1);
            memcpy(&a2, code + PC, sizeof(uint32_t));
            PC += sizeof(uint32_t);
            fprintf(f, "LINE %u, \"", a2);
            memcpy(&a2, code + PC, sizeof(uint32_t));
            PC += sizeof(uint32_t);
            fprintf(f, "%s\"\n", (const char*)(strs + a2));
            break;
        case JIG_INSTR_PICK:
            fprintf(f, "0x%04X    PICK\n", PC-1);
            break;
        case JIG_INSTR_EVENT:
            memcpy(&a2, code + PC, sizeof(uint32_t));
            fprintf(f, "0x%04X    EVENT \"%s\"\n", PC-1, (const char*)strs + a2);
            PC += sizeof(uint32_t);
            break;
        }
    }
}

