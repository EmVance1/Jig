#include "jig/jig.h"
#include "jig/vm.h"
#include "jig/bc.h"
#include <string.h>
#include <stdbool.h>


typedef struct _InternalProp {
    uint32_t pos;
    uint8_t idx;
} _InternalProp;
static void _vm_enq_iprop(jigVm* self, _InternalProp p) {
    self->ctx.text[self->ctx.TH++] = (jigProposition){ (const char*)self->strs + p.pos, p.idx };
}
static int _vm_step(jigVm* self, jigVmCtx ctx, void* user_data);


void jig_vm_init(jigVm* self, const jigFile* file, const char* module) {
    self->base = (const uint8_t*)file;
    jigFileModTableEntry entry = jig_file_module(file, module);
    self->code = entry.code;
    self->strs = jig_file_strs(file);
    self->ctx.PC = entry.entry;
    self->ctx.SP = 0;
    self->ctx.TH = 0;
    self->ctx.TT = 0;
    self->ctx.A  = 0;
    self->ctx.B  = 0;
}

const char* jig_vm_line(const jigVm* self) { return (const char*)self->strs + self->ctx.B; }
uint32_t    jig_vm_id(const jigVm* self) { return self->ctx.A; }
uint32_t    jig_vm_nq(const jigVm* self) { return self->ctx.B; }

void           jig_vm_push_value(jigVm* self, uint64_t val) { self->ctx.stack[(self->ctx.SP)++] = val; }
uint64_t       jig_vm_pop_value(jigVm* self)       { return self->ctx.stack[--(self->ctx.SP)]; }
uint64_t       jig_vm_top_value(const jigVm* self) { return self->ctx.stack[self->ctx.SP-1]; }
jigProposition jig_vm_dequeue_text(jigVm* self)    { return self->ctx.text[(self->ctx.TT)++]; }

int jig_vm_exec(jigVm* self, jigVmCtx ctx, void* user_data) {
    self->ctx.TH = 0;
    self->ctx.TT = 0;
    int s = _vm_step(self, ctx, user_data);
    while (s == JIG_UPCALL_NONE) s = _vm_step(self, ctx, user_data);
    return s;
}


static int _vm_step(jigVm* self, jigVmCtx ctx, void* user_data) {
    switch (self->code[self->ctx.PC++]) {
    case JIG_INSTR_EOF:
        return JIG_UPCALL_EXIT;

    case JIG_INSTR_PUSH:
        memcpy(self->ctx.stack + self->ctx.SP++, self->code + self->ctx.PC, sizeof(uint64_t));
        self->ctx.PC += sizeof(uint64_t);
        break;
    case JIG_INSTR_POP:
        --self->ctx.SP;
        break;
    case JIG_INSTR_DUP: {
        const uint64_t val = self->ctx.stack[self->ctx.SP-1];
        self->ctx.stack[self->ctx.SP++] = val;
        break; }

    case JIG_INSTR_LOAD: {
        uint32_t k;
        memcpy(&k, self->code + self->ctx.PC, sizeof(uint32_t));
        self->ctx.stack[self->ctx.SP++] = *ctx(k, user_data);
        self->ctx.PC += sizeof(uint32_t);
        break; }
    case JIG_INSTR_STORE: {
        uint32_t k;
        memcpy(&k, self->code + self->ctx.PC, sizeof(uint32_t));
        *ctx(k, user_data) = self->ctx.stack[--self->ctx.SP];
        self->ctx.PC += sizeof(uint32_t);
        break; }

    case JIG_INSTR_LAND: {
        const uint64_t rhs = self->ctx.stack[--self->ctx.SP];
        const uint64_t lhs = self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs && rhs);
        break; }
    case JIG_INSTR_LOR: {
        const uint64_t rhs = self->ctx.stack[--self->ctx.SP];
        const uint64_t lhs = self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs || rhs);
        break; }
    case JIG_INSTR_LNOT: {
        const uint64_t lhs = self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(!lhs);
        break; }
    case JIG_INSTR_ADD: {
        const uint64_t rhs = self->ctx.stack[--self->ctx.SP];
        const uint64_t lhs = self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs + rhs);
        break; }
    case JIG_INSTR_SUB: {
        const uint64_t rhs = self->ctx.stack[--self->ctx.SP];
        const uint64_t lhs = self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs - rhs);
        break; }
    case JIG_INSTR_MUL: {
        const uint64_t rhs = self->ctx.stack[--self->ctx.SP];
        const uint64_t lhs = self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs * rhs);
        break; }
    case JIG_INSTR_DIV: {
        const uint64_t rhs = self->ctx.stack[--self->ctx.SP];
        const uint64_t lhs = self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs / rhs);
        break; }
    case JIG_INSTR_NEG: {
        const int64_t lhs = (int64_t)self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(-lhs);
        break; }
    case JIG_INSTR_EQU: {
        const uint64_t rhs = self->ctx.stack[--self->ctx.SP];
        const uint64_t lhs = self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs == rhs);
        break; }
    case JIG_INSTR_NEQ: {
        const uint64_t rhs = self->ctx.stack[--self->ctx.SP];
        const uint64_t lhs = self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs != rhs);
        break; }
    case JIG_INSTR_IEQV: {
        const uint64_t lhs = self->ctx.stack[self->ctx.SP-1];
        uint64_t rhs;
        memcpy(&rhs, self->code + self->ctx.PC, sizeof(uint64_t));
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs == rhs);
        self->ctx.PC += sizeof(uint64_t);
        break; }
    case JIG_INSTR_IEQK: {
        const uint64_t lhs = self->ctx.stack[self->ctx.SP-1];
        uint32_t rhs;
        memcpy(&rhs, self->code + self->ctx.PC, sizeof(uint32_t));
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs == *ctx(rhs, user_data));
        self->ctx.PC += sizeof(uint32_t);
        break; }
    case JIG_INSTR_LEQ: {
        const uint64_t rhs = self->ctx.stack[--self->ctx.SP];
        const uint64_t lhs = self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs <= rhs);
        break; }
    case JIG_INSTR_LTH: {
        const uint64_t rhs = self->ctx.stack[--self->ctx.SP];
        const uint64_t lhs = self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs < rhs);
        break; }
    case JIG_INSTR_GEQ: {
        const uint64_t rhs = self->ctx.stack[--self->ctx.SP];
        const uint64_t lhs = self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs >= rhs);
        break; }
    case JIG_INSTR_GTH: {
        const uint64_t rhs = self->ctx.stack[--self->ctx.SP];
        const uint64_t lhs = self->ctx.stack[--self->ctx.SP];
        self->ctx.stack[self->ctx.SP++] = (uint64_t)(lhs > rhs);
        break; }
    case JIG_INSTR_JUMP:
        memcpy(&self->ctx.PC, self->code + self->ctx.PC, sizeof(uint32_t));
        if (self->ctx.PC == JIG_VERTEX_EXIT) return JIG_UPCALL_EXIT;
        break;
    case JIG_INSTR_JZ:
        if (self->ctx.stack[--self->ctx.SP] == 0) {
            memcpy(&self->ctx.PC, self->code + self->ctx.PC, sizeof(uint32_t));
            if (self->ctx.PC == JIG_VERTEX_EXIT) return JIG_UPCALL_EXIT;
        } else {
            self->ctx.PC += sizeof(uint32_t);
        }
        break;
    case JIG_INSTR_JNZ:
        if (self->ctx.stack[--self->ctx.SP] != 0) {
            memcpy(&self->ctx.PC, self->code + self->ctx.PC, sizeof(uint32_t));
            if (self->ctx.PC == JIG_VERTEX_EXIT) return JIG_UPCALL_EXIT;
        } else {
            self->ctx.PC += sizeof(uint32_t);
        }
        break;
    case JIG_INSTR_SWITCH: {
        const uint32_t off = (uint32_t)self->ctx.stack[--self->ctx.SP] * sizeof(uint32_t);
        memcpy(&self->ctx.PC, self->code + self->ctx.PC + off, sizeof(uint32_t));
        if (self->ctx.PC == JIG_VERTEX_EXIT) return JIG_UPCALL_EXIT;
        break; }

    case JIG_INSTR_PROP: {
        _InternalProp p;
        memcpy(&p, self->code + self->ctx.PC, sizeof(uint32_t) + sizeof(uint8_t));
        self->ctx.PC += sizeof(uint32_t) + sizeof(uint8_t);
        _vm_enq_iprop(self, p);
        break; }

    case JIG_INSTR_PICK:
        self->ctx.B = self->ctx.TH;
        return JIG_UPCALL_PICK;
    case JIG_INSTR_LINE:
        memcpy(&self->ctx.A, self->code + self->ctx.PC, sizeof(uint32_t));
        self->ctx.PC += sizeof(uint32_t);
        memcpy(&self->ctx.B, self->code + self->ctx.PC, sizeof(uint32_t));
        self->ctx.PC += sizeof(uint32_t);
        return JIG_UPCALL_LINE;
    case JIG_INSTR_EVENT:
        memcpy(&self->ctx.B, self->code + self->ctx.PC, sizeof(uint32_t));
        self->ctx.PC += sizeof(uint32_t);
        return JIG_UPCALL_EVENT;
    }
    return (self->ctx.SP > 128) ? JIG_UPCALL_ABORT : JIG_UPCALL_NONE;
}

