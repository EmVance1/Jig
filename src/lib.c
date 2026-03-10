#define _CRT_SECURE_NO_WARNINGS
#include "jig/jig.h"
#include "jig/ast.h"
#include "jig/bc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define JIGDS_ALLOCATOR (ctx->alloc)
#include "vec.h"


jigFile* jig_file_internal_merge_blobs(const jigProgramData* pdata, jigAllocator alloc);

jigFile* jig_file_compile_from_src(const char* src, jigError* errors, jigSymbolCtx symbol_ctx, jigAllocator alloc) {
    *errors = (jigError){ 0 };
    jigMemoryArena ast_arena = jig_memory_arena_create(JIG_MEMORY_ARENA_PAGE, 100 * 1000);
    const jigAst ast = jig_ast_parse_from_src(src, errors, jig_memory_arena_allocator(&ast_arena));

    if (errors->list == NULL) {
        jigMemoryArena pdata_arena = jig_memory_arena_create(JIG_MEMORY_ARENA_PAGE, 100 * 1000);
        jigProgramData pdata = jig_compile_ast(&ast, symbol_ctx, jig_memory_arena_allocator(&pdata_arena));
        jig_memory_arena_destroy(&ast_arena);
        jigFile* result = jig_file_internal_merge_blobs(&pdata, alloc);
        jig_memory_arena_destroy(&pdata_arena);
        return result;
    } else {
        jig_memory_arena_destroy(&ast_arena);
        return NULL;
    }
}

jigFile* jig_file_compile_from_ast(const jigAst* ast, jigSymbolCtx symbol_ctx, jigAllocator alloc) {
    jigMemoryArena arena = jig_memory_arena_create(JIG_MEMORY_ARENA_PAGE, 100 * 1000);
    jigProgramData pdata = jig_compile_ast(ast, symbol_ctx, jig_memory_arena_allocator(&arena));
    jigFile* result = jig_file_internal_merge_blobs(&pdata, alloc);
    jig_memory_arena_destroy(&arena);
    return result;
}

jigFile* jig_file_internal_merge_blobs(const jigProgramData* pdata, jigAllocator alloc) {
    const size_t meta_size = sizeof(jigFileHeader) + jigds_arrlenu(pdata->syms);
    size_t mods_size = 0;
    size_t code_size = 0;
    size_t strs_size = jigds_arrlenu(pdata->strs);
    for (size_t i = 0; i < jigds_arrlenu(pdata->mods); i++) {
        mods_size += 4 * sizeof(uint32_t);
        code_size += jigds_arrlenu(pdata->mods[i].code);
        strs_size += (pdata->mods[i].name.len + 1);
    }
    const size_t file_size = meta_size + mods_size + code_size + strs_size;

    uint8_t* base_ptr = jig_allocator_allocate(alloc, file_size);
    jigFileHeader* header = (jigFileHeader*)base_ptr;
    *header = (jigFileHeader){
        .magic={ 'q', 'u', 'o', 's', 'i' },
        .majver=VANGO_PKG_VERSION_MAJOR,
        .minver=VANGO_PKG_VERSION_MINOR,
        .patch =VANGO_PKG_VERSION_PATCH,
        .fsize =(uint32_t)file_size,
        .nmods =(uint32_t)jigds_arrlenu(pdata->mods),
        .code_pos=(uint32_t)(sizeof(jigFileHeader) + mods_size),
        .strs_pos=(uint32_t)(sizeof(jigFileHeader) + mods_size + code_size),
        .syms_pos=(uint32_t)(sizeof(jigFileHeader) + mods_size + code_size + strs_size),
    };

    uint8_t* current = base_ptr + sizeof(jigFileHeader);
    uint32_t code_pos = header->code_pos;
    uint32_t name_pos = header->strs_pos + (uint32_t)jigds_arrlenu(pdata->strs);
    for (size_t i = 0; i < jigds_arrlenu(pdata->mods); i++) {
        const jigModData* g = &pdata->mods[i];
        const uint32_t code_len = (uint32_t)jigds_arrlenu(g->code);
        memcpy(current,                        &name_pos, sizeof(uint32_t));
        memcpy(current + 1 * sizeof(uint32_t), &code_pos, sizeof(uint32_t));
        memcpy(current + 2 * sizeof(uint32_t), &code_len, sizeof(uint32_t));
        memcpy(current + 3 * sizeof(uint32_t), &g->entry, sizeof(uint32_t));
        current += 4 * sizeof(uint32_t);
        code_pos += code_len;

        memcpy(base_ptr + name_pos, g->name.ptr, g->name.len);
        base_ptr[name_pos + g->name.len] = 0;

        name_pos += (uint32_t)g->name.len + 1;
    }

    current = base_ptr + header->code_pos;
    for (size_t i = 0; i < jigds_arrlenu(pdata->mods); i++) {
        const jigModData* g = &pdata->mods[i];
        const size_t code_len = jigds_arrlenu(g->code);
        memcpy(current, g->code, code_len);
        current += code_len;
    }

    memcpy(base_ptr + header->strs_pos, pdata->strs, jigds_arrlenu(pdata->strs));
    memcpy(base_ptr + header->syms_pos, pdata->syms, jigds_arrlenu(pdata->syms));
    return (jigFile*)base_ptr;
}


const jigFileHeader* jig_file_header(const jigFile* file) {
    return (jigFileHeader*)file;
}
jigFileModTableEntry jig_file_module(const jigFile* file, const char* module) {
    const uint8_t* base_ptr = (const uint8_t*)file;
    const uint8_t* ptr = jig_file_mod_table(file);
    for (uint32_t i = 0; i < jig_file_header(file)->nmods; i++) {
        uint32_t name_pos, code_pos, code_len, code_beg;
        memcpy(&name_pos, ptr,                        sizeof(uint32_t));
        memcpy(&code_pos, ptr + 1 * sizeof(uint32_t), sizeof(uint32_t));
        memcpy(&code_len, ptr + 2 * sizeof(uint32_t), sizeof(uint32_t));
        memcpy(&code_beg, ptr + 3 * sizeof(uint32_t), sizeof(uint32_t));
        const char* name = (const char*)base_ptr + name_pos;
        const size_t name_len = strlen((const char*)ptr);
        if (strncmp(name, module, name_len+1) == 0) {
            return (jigFileModTableEntry){ .code=base_ptr+code_pos, .len=code_len, .entry=code_beg };
        }
        ptr += 4 * sizeof(uint32_t);
    }
    return (jigFileModTableEntry){ 0 };
}

const uint8_t* jig_file_end(const jigFile* file) {
    return (uint8_t*)file + jig_file_header(file)->fsize;
}
size_t jig_file_len(const jigFile* file) {
    return jig_file_header(file)->fsize;
}

const uint8_t* jig_file_mod_table(const jigFile* file) {
    return (uint8_t*)file + sizeof(jigFileHeader);
}
size_t jig_file_mod_table_len(const jigFile* file) {
    return (size_t)(jig_file_code(file) - jig_file_mod_table(file));
}
const uint8_t* jig_file_code(const jigFile* file) {
    return (uint8_t*)file + jig_file_header(file)->code_pos;
}
size_t jig_file_code_len(const jigFile* file) {
    return (size_t)(jig_file_strs(file) - jig_file_code(file));
}
const uint8_t* jig_file_strs(const jigFile* file) {
    return (uint8_t*)file + jig_file_header(file)->strs_pos;
}
size_t jig_file_strs_len(const jigFile* file) {
    return (size_t)(jig_file_syms(file) - jig_file_strs(file));
}
const uint8_t* jig_file_syms(const jigFile* file) {
    return (uint8_t*)file + jig_file_header(file)->syms_pos;
}
size_t jig_file_syms_len(const jigFile* file) {
    return (size_t)(jig_file_end(file) - jig_file_syms(file));
}

