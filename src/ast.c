#include "jig/ast.h"
#include "jig/bc.h"
#include "jig/jig.h"
#define JIGDS_ALLOCATOR alloc
#include "vec.h"
#include <stdio.h>


static void jig_expr_free(jigExpr* expr, jigAllocator alloc) {
    if (expr->tag == JIG_EXPR_OP) {
        jig_expr_free(expr->lhs, alloc);
        jig_allocator_deallocate(alloc, expr->lhs);
        if (expr->value.op != JIG_INSTR_NEG && expr->value.op != JIG_INSTR_LNOT) {
            jig_expr_free(expr->rhs, alloc);
            jig_allocator_deallocate(alloc, expr->rhs);
        }
    }
}

static void jig_edgeblock_free(jigEdgeBlock* block, jigAllocator alloc);
static void jig_edge_free(jigEdge* edge, jigAllocator alloc) {
    for (size_t i = 0; i < jigds_arrlenu(edge->effects); i++) {
        if (edge->effects[i].op != JIG_EFFECT_EVENT) {
            jig_expr_free(&edge->effects[i].rhs, alloc);
        }
    }
    jigds_arrfree(edge->effects);
}
static void jig_edgematch_free(jigEdgeMatch* match, jigAllocator alloc) {
    jig_expr_free(&match->expr, alloc);
    for (size_t i = 0; i < jigds_arrlenu(match->arms); i++) {
        jig_expr_free(&match->arms[i].cond, alloc);
        jig_edge_free(&match->arms[i].body, alloc);
    }
    jigds_arrfree(match->arms);
    if (match->catchall.exists) {
        jig_edge_free(&match->catchall.arm, alloc);
    }
}
static void jig_edgeifelse_free(jigEdgeIfElse* ifelse, jigAllocator alloc) {
    for (size_t i = 0; i < jigds_arrlenu(ifelse->blocks); i++) {
        jig_expr_free(&ifelse->blocks[i].cond, alloc);
        for (size_t j = 0; j < jigds_arrlenu(ifelse->blocks[i].body); j++) {
            jig_edgeblock_free(&ifelse->blocks[i].body[j], alloc);
        }
        jigds_arrfree(ifelse->blocks[i].body);
    }
    jigds_arrfree(ifelse->blocks);
    if (ifelse->catchall) {
        for (size_t i = 0; i < jigds_arrlenu(ifelse->catchall); i++) {
            jig_edgeblock_free(&ifelse->catchall[i], alloc);
        }
        jigds_arrfree(ifelse->catchall);
    }
}

static void jig_edgeblock_free(jigEdgeBlock* block, jigAllocator alloc) {
    switch (block->tag) {
    case JIG_EBLOCK_T:
        for (size_t i = 0; i < jigds_arrlenu(block->value.edges); i++) {
            jig_edge_free(&block->value.edges[i], alloc);
        }
        jigds_arrfree(block->value.edges);
        break;
    case JIG_EBLOCK_MATCH:
        jig_edgematch_free(&block->value.match, alloc);
        break;
    case JIG_EBLOCK_IFELSE:
        jig_edgeifelse_free(&block->value.ifelse, alloc);
        break;
    }
}

static void jig_vertblock_free(jigVertexBlock* block, jigAllocator alloc);
static void jig_vertex_free(jigVertex* vert, jigAllocator alloc) {
    for (size_t i = 0; i < jigds_arrlenu(vert->lineset); i++) {
        jigds_arrfree(vert->lineset[i].lines);
    }
    jigds_arrfree(vert->lineset);
    for (size_t i = 0; i < jigds_arrlenu(vert->v.edges); i++) {
        jig_edgeblock_free(&vert->v.edges[i], alloc);
    }
    if (vert->type == JIG_VERTEX_JUMP) {
        jigds_arrfree(vert->v.jump.effects);
    } else {
        jigds_arrfree(vert->v.edges);
    }
}
static void jig_vertmatch_free(jigVertexMatch* match, jigAllocator alloc) {
    jig_expr_free(&match->expr, alloc);
    for (size_t i = 0; i < jigds_arrlenu(match->arms); i++) {
        jig_expr_free(&match->arms[i].cond, alloc);
        jig_vertex_free(&match->arms[i].body, alloc);
    }
    jigds_arrfree(match->arms);
    jig_vertex_free(&match->catchall, alloc);
}
static void jig_vertifelse_free(jigVertexIfElse* ifelse, jigAllocator alloc) {
    for (size_t i = 0; i < jigds_arrlenu(ifelse->blocks); i++) {
        jig_expr_free(&ifelse->blocks[i].cond, alloc);
        jig_vertblock_free(ifelse->blocks[i].data, alloc);
        jig_allocator_deallocate(alloc, ifelse->blocks[i].data);
    }
    jigds_arrfree(ifelse->blocks);
    jig_vertblock_free(ifelse->catchall, alloc);
    jig_allocator_deallocate(alloc, ifelse->catchall);
}

static void jig_vertblock_free(jigVertexBlock* block, jigAllocator alloc) {
    switch (block->tag) {
    case JIG_VBLOCK_T:
        jig_vertex_free(&block->value.vertex, alloc);
        break;
    case JIG_VBLOCK_MATCH:
        jig_vertmatch_free(&block->value.match, alloc);
        break;
    case JIG_VBLOCK_IFELSE:
        jig_vertifelse_free(&block->value.ifelse, alloc);
        break;
    }
}

static void jig_graph_free(jigGraph* graph, jigAllocator alloc) {
    jigds_arrfree(graph->databases);
    for (size_t i = 0; i < jigds_arrlenu(graph->vertices); i++) {
        jig_vertblock_free(&graph->vertices[i].data, alloc);
    }
    jigds_arrfree(graph->vertices);
}

void jig_ast_free(const jigAst* _ast, jigAllocator alloc) {
    jigAst* ast = (jigAst*)_ast;
    for (size_t i = 0; i < jigds_arrlenu(ast->modules); i++) {
        jig_graph_free(&ast->modules[i], alloc);
    }
    jigds_arrfree(ast->modules);
}

