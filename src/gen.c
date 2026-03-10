#include "jig/jig.h"
#include "jig/ast.h"
#include "jig/bc.h"
#include <string.h>
#include <stdio.h>
#define JIGDS_ALLOCATOR (ctx->alloc)
#include "vec.h"


#define STREQ(view, cptr) ((view.len == sizeof(cptr)-1) && (strncmp(view.ptr, cptr, view.len) == 0))


typedef struct JumpTarget {
    uint32_t referenced_at;
    uint32_t label;
} LabelTarget;
typedef struct StringTarget {
    uint32_t referenced_at;
    jigStrView value;
} StringTarget;
typedef struct EffectTarget {
    const jigEdge* edge;
    uint32_t label;
} EffectTarget;

typedef struct GenContext {
    jigAllocator alloc;
    jigSymbolCtx symbol_ctx;
    const jigGraph* name_lkp;

    // vector
    uint8_t* result;
    // vector
    uint32_t* labels;
    // vector
    LabelTarget* jumps;
    // vector
    EffectTarget* edges;
    // vector
    StringTarget* strings;

    uint32_t edge_index;
    uint32_t symbol_index;

    // std::pmr::unordered_map<std::string_view, uint32_t> symbols;
} GenContext;

void jig_program_data_free(jigProgramData* data, jigAllocator alloc) {
    struct DummyContext { jigAllocator alloc; } context;
    struct DummyContext* ctx = &context;
    ctx->alloc = alloc;

    for (size_t i = 0; i < jigds_arrlenu(data->mods); i++) {
        jigds_arrfree(data->mods[i].code);
    }
    jigds_arrfree(data->mods);
    jigds_arrfree(data->strs);
    jigds_arrfree(data->syms);
}

static uint32_t gen_label(GenContext* ctx) {
    const uint32_t l = (uint32_t)jigds_arrlenu(ctx->labels);
    jigds_arraddn(ctx->labels, 1);
    return l;
}
static uint32_t resolve_edge(GenContext* ctx, jigStrView edge) {
    if (STREQ(edge, "EXIT")) {
        return UINT32_MAX;
    }
    for (size_t i = 0; i < jigds_arrlenu(ctx->name_lkp->vertices); i++) {
        if ((ctx->name_lkp->vertices[i].name.len == edge.len) &&
            (strncmp(ctx->name_lkp->vertices[i].name.ptr, edge.ptr, edge.len) == 0))
        {
            return (uint32_t)i;
        }
    }
    return 0;
}
static uint32_t resolve_flag(GenContext* ctx, jigStrView sym) {
    char* cpy = jig_allocator_allocate(ctx->alloc, (sym.len+1) * sizeof(char));
    cpy[sym.len] = 0;
    memcpy(cpy, sym.ptr, sym.len);
    return ctx->symbol_ctx.data_lkp(cpy, ctx->symbol_ctx.data_context);
}
static uint32_t resolve_speaker(GenContext* ctx, jigStrView sym) {
    char* cpy = jig_allocator_allocate(ctx->alloc, (sym.len+1) * sizeof(char));
    cpy[sym.len] = 0;
    memcpy(cpy, sym.ptr, sym.len);
    return ctx->symbol_ctx.speaker_lkp(cpy, ctx->symbol_ctx.speaker_context);
}


static void compile_edge(GenContext* ctx, const jigEdge* edge);
static void compile_vertex(GenContext* ctx, const jigVertex* vert);
static void compile_effects(GenContext* ctx, const jigEffect* effs);
static void compile_expr(GenContext* ctx, const jigExpr* expr, bool ieq);
static void compile_eblock(GenContext* ctx, const jigEdgeBlock* block);
static void compile_vblock(GenContext* ctx, const jigVertexBlock* block);


jigProgramData jig_compile_ast(const jigAst* ast, jigSymbolCtx symbol_ctx, jigAllocator alloc) {
    GenContext context = (GenContext){
        .alloc=alloc,
        .symbol_ctx=symbol_ctx,
    };
    GenContext* ctx = &context;
    jigProgramData result = {0};

    for (size_t i = 0; i < jigds_arrlenu(ast->modules); i++) {
        const jigGraph* mod = &ast->modules[i];
        ctx->name_lkp = mod;
        ctx->result = NULL;
        ctx->labels = NULL;
        ctx->jumps = NULL;
        ctx->edges = NULL;
        ctx->strings = NULL;
        ctx->edge_index = 0;
        ctx->symbol_index = 0;

        // FIRST #VERTICES LABELS RESERVED FOR EDGE JUMPS
        jigds_arraddn(ctx->labels, jigds_arrlenu(mod->vertices));
        uint32_t entry_pos = UINT32_MAX;

        // CODE GENERATION
        for (size_t j = 0; j < jigds_arrlenu(mod->vertices); j++) {
            const jigNamedVertex* v = &mod->vertices[j];
            const uint32_t curr_pos = (uint32_t)jigds_arrlenu(ctx->result);
            ctx->labels[j] = curr_pos;
            if (STREQ(v->name, "START")) entry_pos = curr_pos;
            compile_vblock(ctx, &v->data);
        }

        // JUMP PATCHING
        for (size_t j = 0; j < jigds_arrlenu(ctx->jumps); j++) {
            const LabelTarget* jmp = &ctx->jumps[j];
            if (jmp->label == UINT32_MAX) {
                const uint32_t exit_marker = UINT32_MAX;
                memcpy(ctx->result + jmp->referenced_at, &exit_marker, sizeof(uint32_t));
            } else {
                memcpy(ctx->result + jmp->referenced_at, &ctx->labels[jmp->label], sizeof(uint32_t));
            }
        }

        // STRING PATCHING
        for (size_t j = 0; j < jigds_arrlenu(ctx->strings); j++) {
            const StringTarget* str = &ctx->strings[j];
            const uint32_t pos = (uint32_t)jigds_arrlenu(result.strs);
            memcpy(ctx->result + str->referenced_at, &pos, sizeof(uint32_t));

            bool esc = false;
            for (size_t k = 0; k < str->value.len; k++) {
                const char c = str->value.ptr[k];
                if (esc) {
                    switch (c) {
                    case 'n':  jigds_arrpush(result.strs, '\n'); break;
                    case '"':  jigds_arrpush(result.strs, '"');  break;
                    case '\\': jigds_arrpush(result.strs, '\\'); break;
                    }
                    esc = false;
                } else if (c == '\\') {
                    esc = true;
                } else {
                    jigds_arrpush(result.strs, (uint8_t)c);
                }
            }
            jigds_arrpush(result.strs, (uint8_t)0);
        }

        // DO NOT FREE RESULT -> MUST BE ALIVE AT LOAD TIME
        jigds_arrfree(ctx->labels);
        jigds_arrfree(ctx->jumps);
        jigds_arrfree(ctx->edges);
        jigds_arrfree(ctx->strings);
        jigds_arrpush(result.mods, ((jigModData){ .name=mod->name, .entry=entry_pos, .code=ctx->result }));
    }

    /* symbol table */
    /*
    if (!symbol_ctx) {
        for (size_t i = 0; i < jigds_arrlenu(result.syms); i++) {
            for (const auto c : k) {
                jigds_arrpush(result.syms, c);
            }
            jigds_arrpush(result.syms, (uint8_t)0);
            jigds_arrpush(result.syms, v);
        }
    }
    */

    return result;
}


static void compile_expr(GenContext* ctx, const jigExpr* e, bool ieq) {
    switch (e->tag) {
    case JIG_EXPR_IDENT: {
        jigds_arrpush(ctx->result, (uint8_t)(ieq ? JIG_INSTR_IEQK : JIG_INSTR_LOAD));
        const uint32_t ref = resolve_flag(ctx, e->value.ident);
        uint8_t* begin = jigds_arraddnptr(ctx->result, sizeof(uint32_t));
        memcpy(begin, &ref, sizeof(uint32_t));
        break; }
    case JIG_EXPR_IMM: {
        jigds_arrpush(ctx->result, (uint8_t)(ieq ? JIG_INSTR_IEQV : JIG_INSTR_PUSH));
        uint8_t* begin = jigds_arraddnptr(ctx->result, sizeof(uint64_t));
        memcpy(begin, &e->value.imm, sizeof(uint64_t));
        break; }
    case JIG_EXPR_OP:
        if (e->value.op == JIG_INSTR_LNOT) {
            compile_expr(ctx, e->lhs, false);
            jigds_arrpush(ctx->result, e->value.op);
        } else if (e->value.op == JIG_INSTR_STORE) {
            compile_expr(ctx, e->rhs, false);
            jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_DUP);
            jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_STORE);
            const uint32_t ref = resolve_flag(ctx, e->lhs->value.ident);
            uint8_t* begin = jigds_arraddnptr(ctx->result, sizeof(uint32_t));
            memcpy(begin, &ref, sizeof(uint32_t));
        } else {
            compile_expr(ctx, e->lhs, false);
            compile_expr(ctx, e->rhs, false);
            jigds_arrpush(ctx->result, e->value.op);
        }
        break;
    }
}
static void compile_effects(GenContext* ctx, const jigEffect* actions) {
    for (size_t i = 0; i < jigds_arrlenu(actions); i++) {
        const jigEffect* e = &actions[i];
        if (e->op == JIG_EFFECT_EVENT) {
            jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_EVENT);
            jigds_arrpush(ctx->strings, ((StringTarget){ (uint32_t)jigds_arrlenu(ctx->result), e->lhs }));
            jigds_arraddn(ctx->result, sizeof(uint32_t));
        } else {
            if (e->op != JIG_EFFECT_SET) {
                jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_LOAD);
                const uint32_t ref = resolve_flag(ctx, e->lhs);
                uint8_t* begin = jigds_arraddnptr(ctx->result, sizeof(uint32_t));
                memcpy(begin, &ref, sizeof(uint32_t));
            }
            compile_expr(ctx, &e->rhs, false);
            switch (e->op) {
            case JIG_EFFECT_ADD:
                jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_ADD);
                break;
            case JIG_EFFECT_SUB:
                jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_SUB);
                break;
            case JIG_EFFECT_MUL:
                jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_MUL);
                break;
            case JIG_EFFECT_DIV:
                jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_DIV);
                break;
            default:
                break;
            }
            jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_STORE);
            const uint32_t ref2 = resolve_flag(ctx, e->lhs);
            uint8_t* begin2 = jigds_arraddnptr(ctx->result, sizeof(uint32_t));
            memcpy(begin2, &ref2, sizeof(uint32_t));
        }
    }
}

static void compile_eblock(GenContext* ctx, const jigEdgeBlock* b) {
    switch (b->tag) {
    case JIG_EBLOCK_T:
        for (size_t i = 0; i < jigds_arrlenu(b->value.edges); i++) {
            compile_edge(ctx, &b->value.edges[i]);
        }
        break;
    case JIG_EBLOCK_MATCH: {
        const jigEdgeMatch* mc = &b->value.match;
        const uint32_t end_lbl = gen_label(ctx);
        compile_expr(ctx, &mc->expr, false);
        for (size_t i = 0; i < jigds_arrlenu(mc->arms); i++) {
            const uint32_t next_lbl = gen_label(ctx);
            compile_expr(ctx, &mc->arms[i].cond, true);
            jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_JZ);
            jigds_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)jigds_arrlenu(ctx->result), next_lbl }));

            jigds_arraddn(ctx->result, sizeof(uint32_t));
            compile_edge(ctx, &mc->arms[i].body);
            jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_JUMP);
            jigds_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)jigds_arrlenu(ctx->result), end_lbl }));
            jigds_arraddn(ctx->result, sizeof(uint32_t));
            ctx->labels[next_lbl] = (uint32_t)jigds_arrlenu(ctx->result);
        }
        if (mc->catchall.exists) {
            compile_edge(ctx, &mc->catchall.arm);
        }
        ctx->labels[end_lbl] = (uint32_t)jigds_arrlenu(ctx->result);
        jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_POP);
        break; }
    case JIG_EBLOCK_IFELSE: {
        const jigEdgeIfElse* ie = &b->value.ifelse;
        const uint32_t end_lbl = gen_label(ctx);
        size_t idx = 0;
        for (size_t i = 0; i < jigds_arrlenu(ie->blocks); i++) {
            const uint32_t next_lbl = gen_label(ctx);
            compile_expr(ctx, &ie->blocks[i].cond, false);
            jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_JZ);
            jigds_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)jigds_arrlenu(ctx->result), next_lbl }));
            jigds_arraddn(ctx->result, sizeof(uint32_t));
            for (size_t j = 0; j < jigds_arrlenu(ie->blocks[i].body); j++) {
                compile_eblock(ctx, &ie->blocks[i].body[j]);
            }
            if (ie->catchall != NULL || idx < jigds_arrlenu(ie->blocks) - 1) { // micro-opt for superfluous tail branches
                jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_JUMP);
                jigds_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)jigds_arrlenu(ctx->result), end_lbl }));
                jigds_arraddn(ctx->result, sizeof(uint32_t));
            }
            ctx->labels[next_lbl] = (uint32_t)jigds_arrlenu(ctx->result);
            idx++;
        }
        if (ie->catchall) {
            for (size_t i = 0; i < jigds_arrlenu(ie->catchall); i++) {
                compile_eblock(ctx, &ie->catchall[i]);
            }
        }
        ctx->labels[end_lbl] = (uint32_t)jigds_arrlenu(ctx->result);
        break; }
    }
    // we do not need tail jumps since edges are innately jumps
}
static void compile_vblock(GenContext* ctx, const jigVertexBlock* b) {
    switch (b->tag) {
    case JIG_VBLOCK_T:
        compile_vertex(ctx, &b->value.vertex);
        break;
    case JIG_VBLOCK_MATCH: {
        const jigVertexMatch* mc = &b->value.match;
        compile_expr(ctx, &mc->expr, false);
        for (size_t i = 0; i < jigds_arrlenu(mc->arms); i++) {
            const uint32_t next_lbl = gen_label(ctx);
            compile_expr(ctx, &mc->arms[i].cond, true);
            jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_JZ);
            jigds_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)jigds_arrlenu(ctx->result), next_lbl }));
            jigds_arraddn(ctx->result, sizeof(uint32_t));
            jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_POP);
            compile_vertex(ctx, &mc->arms[i].body);
            ctx->labels[next_lbl] = (uint32_t)jigds_arrlenu(ctx->result);
        }
        jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_POP);
        compile_vertex(ctx, &mc->catchall);
        break; }
    case JIG_VBLOCK_IFELSE: {
        const jigVertexIfElse* ie = &b->value.ifelse;
        for (size_t i = 0; i < jigds_arrlenu(ie->blocks); i++) {
            const uint32_t next_lbl = gen_label(ctx);
            compile_expr(ctx, &ie->blocks[i].cond, false);
            jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_JZ);
            jigds_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)jigds_arrlenu(ctx->result), next_lbl }));
            jigds_arraddn(ctx->result, sizeof(uint32_t));
            compile_vblock(ctx, ie->blocks[i].data);
            ctx->labels[next_lbl] = (uint32_t)jigds_arrlenu(ctx->result);
        }
        compile_vblock(ctx, ie->catchall);
        break; }
    }
    // we do not need tail jumps since vertices always lead to jumps eventually
}
static void compile_edge(GenContext* ctx, const jigEdge* e) {
    jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_PROP);
    jigds_arrpush(ctx->strings, ((StringTarget){ (uint32_t)jigds_arrlenu(ctx->result), e->line }));
    jigds_arraddn(ctx->result, sizeof(uint32_t));
    jigds_arrpush(ctx->result, (uint8_t)ctx->edge_index++);
    if (e->effects == NULL) {
        jigds_arrpush(ctx->edges, ((EffectTarget){ e, resolve_edge(ctx, e->next) }));
    } else {
        jigds_arrpush(ctx->edges, ((EffectTarget){ e, gen_label(ctx) }));
    }
}
static void compile_vertex(GenContext* ctx, const jigVertex* v) {
    for (size_t i = 0; i < jigds_arrlenu(v->lineset); i++) {
        for (size_t j = 0; j < jigds_arrlenu(v->lineset[i].lines); j++) {
            jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_LINE);
            const uint32_t id = resolve_speaker(ctx, v->lineset[i].speaker);
            uint8_t* begin = jigds_arraddnptr(ctx->result, sizeof(uint32_t));
            memcpy(begin, &id, sizeof(uint32_t));         // speaker ID

            jigds_arrpush(ctx->strings, ((StringTarget){ (uint32_t)(jigds_arrlenu(ctx->result)), v->lineset[i].lines[j] }));
            jigds_arraddn(ctx->result, sizeof(uint32_t)); // line
        }
    }

    if (v->type == JIG_VERTEX_JUMP) {
        if (v->v.jump.effects) {
            compile_effects(ctx, v->v.jump.effects);
        }
        jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_JUMP);
        jigds_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)jigds_arrlenu(ctx->result), resolve_edge(ctx, v->v.jump.next) }));
        jigds_arraddn(ctx->result, sizeof(uint32_t));
    } else {
        ctx->edge_index = 0;
        jigds_arrfree(ctx->edges);
        for (size_t i = 0; i < jigds_arrlenu(v->v.edges); i++) {
            compile_eblock(ctx, &v->v.edges[i]);
        }
        jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_PICK);
        jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_SWITCH);
        for (size_t i = 0; i < jigds_arrlenu(ctx->edges); i++) {
            const EffectTarget* e = &ctx->edges[i];
            jigds_arrpush(ctx->jumps, ((LabelTarget){ (uint32_t)jigds_arrlenu(ctx->result), e->label }));
            jigds_arraddn(ctx->result, sizeof(uint32_t));
        }
        for (size_t i = 0; i < jigds_arrlenu(ctx->edges); i++) {
            const EffectTarget* e = &ctx->edges[i];
            if (e->edge->effects != NULL) {
                ctx->labels[e->label] = (uint32_t)jigds_arrlenu(ctx->result);
                compile_effects(ctx, e->edge->effects);
                jigds_arrpush(ctx->result, (uint8_t)JIG_INSTR_JUMP);
                jigds_arrpush(ctx->jumps,  ((LabelTarget){ (uint32_t)jigds_arrlenu(ctx->result), resolve_edge(ctx, e->edge->next) }));
                jigds_arraddn(ctx->result, sizeof(uint32_t));
            }
        }
    }
}

