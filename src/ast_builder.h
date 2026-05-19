#ifndef AST_BUILDER_H
#define AST_BUILDER_H

#include "ast.h"
#include "parse_tree.h"

AstNode buildAstFromParseTree(const ParseNode &parseTree);

#endif // AST_BUILDER_H
