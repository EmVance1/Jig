#ifndef CQUOSI_IMPL_AST_H
#define CQUOSI_IMPL_AST_H
#include "quosi/quosi.h"
#include <stdint.h>
#include <stdbool.h>


struct quosiExpr;
struct quosiEffect;

struct quosiVertex;
struct quosiEdge;

struct quosiVertexIfElseBlock;
struct quosiVertexIfElse;
struct quosiVertexMatchArm;
struct quosiVertexMatch;

struct quosiEdgeIfElseBlock;
struct quosiEdgeIfElse;
struct quosiEdgeMatchArm;
struct quosiEdgeMatch;

struct quosiVertexLineSet;
struct quosiEdgeBlock;
struct quosiVertexBlock;


typedef struct quosiExpr   quosiExpr;
typedef struct quosiEffect quosiEffect;

typedef struct quosiVertex quosiVertex;
typedef struct quosiEdge   quosiEdge;

typedef struct quosiVertexIfElseBlock quosiVertexIfElseBlock;
typedef struct quosiVertexIfElse      quosiVertexIfElse;
typedef struct quosiVertexMatchArm    quosiVertexMatchArm;
typedef struct quosiVertexMatch       quosiVertexMatch;

typedef struct quosiEdgeIfElseBlock quosiEdgeIfElseBlock;
typedef struct quosiEdgeIfElse      quosiEdgeIfElse;
typedef struct quosiEdgeMatchArm    quosiEdgeMatchArm;
typedef struct quosiEdgeMatch       quosiEdgeMatch;

typedef struct quosiVertexLineSet quosiVertexLineSet;
typedef struct quosiEdgeBlock     quosiEdgeBlock;
typedef struct quosiVertexBlock   quosiVertexBlock;


struct quosiExpr {
    enum quosiExprType { QUOSI_EXPR_IDENT=0, QUOSI_EXPR_IMM, QUOSI_EXPR_OP } tag;
    union {
        quosiStrView ident;
        uint64_t imm;
        uint8_t op;
    } value;
    quosiExpr* lhs;
    quosiExpr* rhs;
};

struct quosiEffect {
    enum quosiEffectType { QUOSI_EFFECT_EVENT=0, QUOSI_EFFECT_ADD, QUOSI_EFFECT_SUB, QUOSI_EFFECT_MUL, QUOSI_EFFECT_DIV, QUOSI_EFFECT_SET } op;
    quosiStrView lhs;
    quosiExpr rhs;
};


struct quosiVertexLineSet {
    quosiStrView speaker;
    // vector
    quosiStrView* lines;
};

struct quosiVertex {
    // vector
    quosiVertexLineSet* lineset;
    enum quosiVectorType { QUOSI_VERTEX_CHOICE=0, QUOSI_VERTEX_JUMP } type;
    union {
        // vector
        quosiEdgeBlock* edges;
        struct {
            // vector
            quosiEffect* effects;
            quosiStrView next;
        } jump;
    } v;
};

struct quosiEdge {
    quosiStrView line;
    quosiStrView next;
    // vector
    quosiEffect* effects;
};


struct quosiVertexIfElseBlock {
    quosiExpr cond;
    quosiVertexBlock* data;
};

struct quosiVertexIfElse {
    // vector
    quosiVertexIfElseBlock* blocks;
    quosiVertexBlock* catchall;
};

struct quosiVertexMatchArm {
    quosiExpr cond;
    quosiVertex body;
};

struct quosiVertexMatch {
    quosiExpr expr;
    // vector
    quosiVertexMatchArm* arms;
    quosiVertex catchall;
};


struct quosiEdgeIfElseBlock {
    quosiExpr cond;
    // vector
    quosiEdgeBlock* body;
};

struct quosiEdgeIfElse {
    // vector
    quosiEdgeIfElseBlock* blocks;
    // vector
    quosiEdgeBlock* catchall;
};

struct quosiEdgeMatchArm {
    quosiExpr cond;
    quosiEdge body;
};

struct quosiEdgeMatch {
    quosiExpr expr;
    // vector
    quosiEdgeMatchArm* arms;
    struct {
        quosiEdge arm;
        bool exists;
    } catchall;
};


struct quosiVertexBlock {
    enum quosiVblockType { QUOSI_VBLOCK_T=0, QUOSI_VBLOCK_MATCH, QUOSI_VBLOCK_IFELSE } tag;
    union {
        quosiVertex vertex;
        quosiVertexMatch match;
        quosiVertexIfElse ifelse;
    } value;
};

struct quosiEdgeBlock {
    enum quosiEblockType { QUOSI_EBLOCK_T=0, QUOSI_EBLOCK_MATCH, QUOSI_EBLOCK_IFELSE } tag;
    union {
        // vector
        quosiEdge* edges;
        quosiEdgeMatch match;
        quosiEdgeIfElse ifelse;
    } value;
};


typedef struct quosiNamedVertex {
    quosiStrView name;
    quosiVertexBlock data;
} quosiNamedVertex;


typedef struct quosiGraph {
    quosiStrView name;
    // vector
    quosiNamedVertex* vertices;
    // std::pmr::unordered_map<std::string_view, std::string_view> rename_table;
} quosiGraph;

typedef struct quosiAst {
    // vector
    quosiGraph* modules;
} quosiAst;


quosiAst quosi_ast_parse_from_src(const char* src, quosiError* errors, quosiAllocator alloc);
void quosi_ast_free(const quosiAst* ast, quosiAllocator alloc);


#endif
