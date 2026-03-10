#ifndef CJIG_IMPL_AST_H
#define CJIG_IMPL_AST_H
#include "jig/jig.h"
#include <stdint.h>
#include <stdbool.h>


struct jigExpr;
struct jigEffect;

struct jigVertex;
struct jigEdge;

struct jigVertexIfElseBlock;
struct jigVertexIfElse;
struct jigVertexMatchArm;
struct jigVertexMatch;

struct jigEdgeIfElseBlock;
struct jigEdgeIfElse;
struct jigEdgeMatchArm;
struct jigEdgeMatch;

struct jigVertexLineSet;
struct jigEdgeBlock;
struct jigVertexBlock;


typedef struct jigExpr   jigExpr;
typedef struct jigEffect jigEffect;

typedef struct jigVertex jigVertex;
typedef struct jigEdge   jigEdge;

typedef struct jigVertexIfElseBlock jigVertexIfElseBlock;
typedef struct jigVertexIfElse      jigVertexIfElse;
typedef struct jigVertexMatchArm    jigVertexMatchArm;
typedef struct jigVertexMatch       jigVertexMatch;

typedef struct jigEdgeIfElseBlock jigEdgeIfElseBlock;
typedef struct jigEdgeIfElse      jigEdgeIfElse;
typedef struct jigEdgeMatchArm    jigEdgeMatchArm;
typedef struct jigEdgeMatch       jigEdgeMatch;

typedef struct jigVertexLineSet jigVertexLineSet;
typedef struct jigEdgeBlock     jigEdgeBlock;
typedef struct jigVertexBlock   jigVertexBlock;


struct jigExpr {
    enum jigExprType { JIG_EXPR_IDENT=0, JIG_EXPR_IMM, JIG_EXPR_OP } tag;
    union {
        jigStrView ident;
        uint64_t imm;
        uint8_t op;
    } value;
    jigExpr* lhs;
    jigExpr* rhs;
};

struct jigEffect {
    enum jigEffectType { JIG_EFFECT_EVENT=0, JIG_EFFECT_ADD, JIG_EFFECT_SUB, JIG_EFFECT_MUL, JIG_EFFECT_DIV, JIG_EFFECT_SET } op;
    jigStrView lhs;
    jigExpr rhs;
};


struct jigVertexLineSet {
    jigStrView speaker;
    // vector
    jigStrView* lines;
};

struct jigVertex {
    // vector
    jigVertexLineSet* lineset;
    enum jigVectorType { JIG_VERTEX_CHOICE=0, JIG_VERTEX_JUMP } type;
    union {
        // vector
        jigEdgeBlock* edges;
        struct {
            // vector
            jigEffect* effects;
            jigStrView next;
        } jump;
    } v;
};

struct jigEdge {
    jigStrView line;
    jigStrView next;
    // vector
    jigEffect* effects;
};


struct jigVertexIfElseBlock {
    jigExpr cond;
    jigVertexBlock* data;
};

struct jigVertexIfElse {
    // vector
    jigVertexIfElseBlock* blocks;
    jigVertexBlock* catchall;
};

struct jigVertexMatchArm {
    jigExpr cond;
    jigVertex body;
};

struct jigVertexMatch {
    jigExpr expr;
    // vector
    jigVertexMatchArm* arms;
    jigVertex catchall;
};


struct jigEdgeIfElseBlock {
    jigExpr cond;
    // vector
    jigEdgeBlock* body;
};

struct jigEdgeIfElse {
    // vector
    jigEdgeIfElseBlock* blocks;
    // vector
    jigEdgeBlock* catchall;
};

struct jigEdgeMatchArm {
    jigExpr cond;
    jigEdge body;
};

struct jigEdgeMatch {
    jigExpr expr;
    // vector
    jigEdgeMatchArm* arms;
    struct {
        jigEdge arm;
        bool exists;
    } catchall;
};


struct jigVertexBlock {
    enum jigVblockType { JIG_VBLOCK_T=0, JIG_VBLOCK_MATCH, JIG_VBLOCK_IFELSE } tag;
    union {
        jigVertex vertex;
        jigVertexMatch match;
        jigVertexIfElse ifelse;
    } value;
};

struct jigEdgeBlock {
    enum jigEblockType { JIG_EBLOCK_T=0, JIG_EBLOCK_MATCH, JIG_EBLOCK_IFELSE } tag;
    union {
        // vector
        jigEdge* edges;
        jigEdgeMatch match;
        jigEdgeIfElse ifelse;
    } value;
};


typedef struct jigNamedVertex {
    jigStrView name;
    jigVertexBlock data;
} jigNamedVertex;

typedef struct jigDatabase {
    jigStrView name;
    jigStrView alias;
} jigDatabase;


typedef struct jigGraph {
    jigStrView name;
    // vector
    jigDatabase* databases;
    // vector
    jigNamedVertex* vertices;
    // std::pmr::unordered_map<std::string_view, std::string_view> rename_table;
} jigGraph;

typedef struct jigAst {
    // vector
    jigGraph* modules;
} jigAst;


jigAst jig_ast_parse_from_src(const char* src, jigError* errors, jigAllocator alloc);
void jig_ast_free(const jigAst* ast, jigAllocator alloc);


#endif
