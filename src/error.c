#include "jig/jig.h"
#include "vec.h"
#include "lex.h"


void jig_error_list_push(jigError* errors, jigErrorValue err) {
    jigds_arrpush(errors->list, err);
}

size_t jig_error_list_len(const jigError* errors) {
    return jigds_arrlenu(errors->list);
}

void jig_error_list_free(jigError* errors) {
    jigds_arrfree(errors->list);
}


int jig_internal_error_handle(jigError* errs, jigToken tok, int type) {
    const jigErrorValue errv = ((jigErrorValue){ .type=type, .span=tok.span });
    jig_error_list_push(errs, errv);
    if (jig_error_is_critical(errv)) {
        errs->critical = true;
        return 1;
    }
    return 0;
}

int jig_internal_error_check(jigError* errs, jigToken tok, int expect, int failtype) {
    if      (tok.type == JIG_TOKEN_ERROR) return jig_internal_error_handle(errs, tok, JIG_ERR_INVALID_TOKEN);
    else if (tok.type == JIG_TOKEN_EOF)   return jig_internal_error_handle(errs, tok, JIG_ERR_EARLY_EOF);
    else if (expect == JIG_TOKEN_IDENT && tok.type == JIG_TOKEN_KEYWORD) {
        return jig_internal_error_handle(errs, tok, JIG_ERR_RESERVED_NAME);
    }
    else if (tok.type != expect) return jig_internal_error_handle(errs, tok, failtype);
    return 0;
}


const char* jig_error_to_string(jigErrorValue e) {
    switch (e.type) {
    case JIG_ERR_UNREACHABLE:
        return "INTERNAL PROGRAMMING ERROR - UNREACHABLE REACHED";
    case JIG_ERR_UNKNOWN:
        return "TODO: label error";

    case JIG_ERR_EARLY_EOF:
        return "end of file encountered abruptly";
    case JIG_ERR_INVALID_TOKEN:
        return "invalid token encountered";
    case JIG_ERR_MISPLACED_TOKEN:
        return "unexpected token encountered";
    case JIG_ERR_RESERVED_NAME:
        return "identifier here is reserved as a keyword";

    case JIG_ERR_BAD_RENAME:
        return "'rename' declaration is malformed";
    case JIG_ERR_BAD_GRAPH_BEGIN:
        return "expected 'module' declaration";
    case JIG_ERR_BAD_VERTEX_BEGIN:
        return "expected node or 'rename' declaration";
    case JIG_ERR_BAD_VERTEX_BLOCK:
        return "expected 'if', 'match' or node definition";
    case JIG_ERR_BAD_EDGE_BLOCK:
        return "expected 'if', 'match' or edge definition";
    case JIG_ERR_BAD_MATCH_ARM:
        return "invalid match arm definition";

    case JIG_ERR_NO_ENTRY:
        return "must have one initial node 'START'";
    case JIG_ERR_NO_ELSE:
        return "'if' in node definition requires catchall case 'else'";
    case JIG_ERR_NO_CATCHALL:
        return "'match' in node definition requires catchall case '_'";
    case JIG_ERR_DUPLICATE_CASE:
        return "'match' expression canot have duplicate cases";
    case JIG_ERR_DUPLICATE_VERTEX:
        return "cannot have duplicate node name";
    case JIG_ERR_DANGLING_EDGE:
        return "node being pointed to does not exist";

    case JIG_ERR_INVALID_ATOM:
        return "atom in expression must be number, identifier, or (expression)";
    case JIG_ERR_INVALID_OPERATOR:
        return "operator in expression is not a valid operator";
    case JIG_ERR_INVALID_ASSIGN:
        return "left hand side of assignment expression must be an identifier";

    case JIG_ERR_UNCLOSED_PAREN:
        return "opening parenthese must be closed here";
    case JIG_ERR_UNCLOSED_ANGLE:
        return "opening angle brackets must be closed here";
    case JIG_ERR_UNCLOSED_CONDITIONAL:
        return "conditional was opened with no corresponding 'end' label";

    default:
        return "INTERNAL PROGRAMMING ERROR - NON-EXHAUSTIVE SWITCH CASE";
    }
}

bool jig_error_is_critical(jigErrorValue e) {
    switch (e.type) {
    case JIG_ERR_UNKNOWN:
        return true;

    case JIG_ERR_EARLY_EOF:
        return true;
    case JIG_ERR_INVALID_TOKEN:
        return true;
    case JIG_ERR_MISPLACED_TOKEN:
        return true;
    case JIG_ERR_RESERVED_NAME:
        return true;

    case JIG_ERR_BAD_RENAME:
        return true;
    case JIG_ERR_BAD_GRAPH_BEGIN:
        return true;
    case JIG_ERR_BAD_VERTEX_BEGIN:
        return true;
    case JIG_ERR_BAD_VERTEX_BLOCK:
        return true;
    case JIG_ERR_BAD_EDGE_BLOCK:
        return true;
    case JIG_ERR_BAD_MATCH_ARM:
        return true;

    case JIG_ERR_NO_ENTRY:
        return false;
    case JIG_ERR_NO_ELSE:
        return false;
    case JIG_ERR_NO_CATCHALL:
        return false;
    case JIG_ERR_DUPLICATE_CASE:
        return false;
    case JIG_ERR_DUPLICATE_VERTEX:
        return false;
    case JIG_ERR_DANGLING_EDGE:
        return false;

    case JIG_ERR_INVALID_ATOM:
        return true;
    case JIG_ERR_INVALID_OPERATOR:
        return true;
    case JIG_ERR_INVALID_ASSIGN:
        return false;

    case JIG_ERR_UNCLOSED_PAREN:
        return true;
    case JIG_ERR_UNCLOSED_ANGLE:
        return true;
    case JIG_ERR_UNCLOSED_CONDITIONAL:
        return true;

    default:
        return true;
    }
}

