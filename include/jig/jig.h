#ifndef CJIG_LIB_H
#define CJIG_LIB_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef struct jigStrView {
    const char* ptr;
    size_t len;
} jigStrView;

#define JIG_STRVIEW(s) ((jigStrView){ .ptr=s, .len=sizeof(s)-1 })

typedef struct jigErrorSpan {
    uint32_t row;
    uint32_t col;
} jigErrorSpan;

typedef struct jigErrorValue {
    enum jigErrorValueType {
        JIG_ERR_UNREACHABLE = -1,
        JIG_ERR_UNKNOWN = 0,
        JIG_ERR_EARLY_EOF,
        JIG_ERR_INVALID_TOKEN,
        JIG_ERR_MISPLACED_TOKEN,
        JIG_ERR_BAD_RENAME,
        JIG_ERR_BAD_GRAPH_BEGIN,
        JIG_ERR_BAD_VERTEX_BEGIN,
        JIG_ERR_BAD_VERTEX_BLOCK,
        JIG_ERR_BAD_EDGE_BLOCK,
        JIG_ERR_BAD_MATCH_ARM,
        JIG_ERR_NO_ENTRY,
        JIG_ERR_NO_ELSE,
        JIG_ERR_NO_CATCHALL,
        JIG_ERR_DUPLICATE_CASE,
        JIG_ERR_DUPLICATE_VERTEX,
        JIG_ERR_DANGLING_EDGE,
        JIG_ERR_INVALID_ATOM,
        JIG_ERR_INVALID_OPERATOR,
        JIG_ERR_INVALID_ASSIGN,
        JIG_ERR_UNCLOSED_PAREN,
        JIG_ERR_UNCLOSED_ANGLE,
        JIG_ERR_UNCLOSED_CONDITIONAL,
    } type;
    jigErrorSpan span;
} jigErrorValue;

const char* jig_error_to_string(jigErrorValue e);
bool jig_error_is_critical(jigErrorValue e);

// error list result from parsing. if no errors were found, list == NULL.
typedef struct jigError  {
    jigErrorValue* list;
    bool critical;
} jigError;

void jig_error_list_push(jigError* errors, jigErrorValue err);
size_t jig_error_list_len(const jigError* errors);
void jig_error_list_free(jigError* errors);


// polymorphic allocator vtable, primarily for internal use
typedef struct jigAllocator {
    void* impl;
    void*(*allocate)  (void* self, size_t nbytes);
    void (*deallocate)(void* self, void* ptr);
    void*(*reallocate)(void* self, void* oldptr, size_t oldbytes, size_t newbytes);
} jigAllocator;

void* jig_allocator_allocate  (jigAllocator alloc, size_t nbytes);
void  jig_allocator_deallocate(jigAllocator alloc, void* ptr);
void* jig_allocator_reallocate(jigAllocator alloc, void* oldptr, size_t oldbytes, size_t newbytes);

enum jigMemoryArenaSource {
    // enables growing, increased syscall overhead
    JIG_MEMORY_ARENA_PAGE,
     // disables growing, decreased syscall overhead, prefer for smaller workloads
    JIG_MEMORY_ARENA_MALLOC,
};
enum jigMemoryArenaGrowthPolicy {
     // fixed size determined at creation time
    JIG_MEMORY_ARENA_STATIC,
     // grow linearly in increments of 'initial'
    JIG_MEMORY_ARENA_LINEAR,
     // grow geometrically in exponents of 'initial'
    JIG_MEMORY_ARENA_GEOMETRIC,
};
typedef struct jigMemoryArena {
    uint8_t* base_ptr;
    uint8_t* curr_ptr;
    uint8_t* last_ptr;
    size_t blocksize;
    size_t reserve;
    size_t commit;
    int source;
    int policy;
} jigMemoryArena;

typedef struct jigMemoryArenaConfig {
    // initial buffer size of the arena, rounded up to a multiple of the system page size
    size_t initial;
    // number of virtual pages to reserve; does not apply when source == JIG_MEMORY_ARENA_MALLOC
    size_t max_reserve;
    // see 'jigMemoryArenaGrowthPolicy', always STATIC when growth_policy == JIG_MEMORY_ARENA_MALLOC
    int source;
    // defers initial buffer allocation to the first object allocation call
    int lazyinit;
    // growth function of the buffer; does nothing when source == JIG_MEMORY_ARENA_MALLOC
    int growth_policy;
} jigMemoryArenaConfig;

jigMemoryArena jig_memory_arena_create(int source, size_t initial);
jigMemoryArena jig_memory_arena_createex(jigMemoryArenaConfig cfg);
// free all memory belonging to arena
void jig_memory_arena_destroy(jigMemoryArena* self);
// resets all pointers, may free excess buffer space
void jig_memory_arena_reset(jigMemoryArena* self);

jigAllocator jig_memory_arena_allocator(jigMemoryArena* arena);
jigAllocator jig_malloc_allocator(void);


struct jigAst;
struct jigFile;
typedef struct jigFile jigFile;
typedef struct jigSymbolCtx {
    // maps script variables to integer IDs
    uint32_t(*data_lkp)(const char*, void* user_data);
    // maps speaker names to integer IDs
    uint32_t(*speaker_lkp)(const char*, void* user_data);
    // data_lkp context object
    void* data_context;
    // speaker_lkp context object
    void* speaker_context;
} jigSymbolCtx;

// metadata for the compiled binary, always makes up first N bytes of the blob
typedef struct jigFileHeader {
    char magic[5];
    uint16_t majver;
    uint16_t minver;
    uint16_t patch;
    uint32_t fsize;
    uint32_t nmods;
    uint32_t code_pos;
    uint32_t strs_pos;
    uint32_t syms_pos;
} jigFileHeader;

// metadata for a single module, entry is an offset from *code, not *file
typedef struct jigFileModTableEntry {
    const uint8_t* code;
    uint32_t len;
    uint32_t entry;
} jigFileModTableEntry;

// return complete compiled binary as single contiguous blob, including header, module table, modules and strings
jigFile* jig_file_compile_from_src(const char* src, jigError* errors, jigSymbolCtx ctx, jigAllocator alloc);
// return complete compiled binary as single contiguous blob, including header, module table, modules and strings
jigFile* jig_file_compile_from_ast(const struct jigAst* ast, jigSymbolCtx ctx, jigAllocator alloc);
// outputs human readable (asm-like) representation of a single module
void jig_file_prettyprint(const jigFile* file, const char* module, void* stdstream);

const jigFileHeader* jig_file_header(const jigFile* file);
jigFileModTableEntry jig_file_module(const jigFile* file, const char* module);

// one byte past the end of the whole blob
const uint8_t* jig_file_end(const jigFile* file);
size_t jig_file_len(const jigFile* file);

const uint8_t* jig_file_mod_table(const jigFile* file);
size_t jig_file_mod_table_len(const jigFile* file);
const uint8_t* jig_file_code(const jigFile* file);
size_t jig_file_code_len(const jigFile* file);
const uint8_t* jig_file_strs(const jigFile* file);
size_t jig_file_strs_len(const jigFile* file);
const uint8_t* jig_file_syms(const jigFile* file);
size_t jig_file_syms_len(const jigFile* file);


#endif
