#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

#include "../ast.h"
#include "../symbol_table.h"
#include "codegen_context.h"

#include <vector>

struct CodeGeneratorResult {
    CodeGenContext context;
    std::vector<Instruction> code;
};

CodeGeneratorResult generateIntermediateCode(const AstNode &decoratedAst,
                                             const SymbolTable &symbols);

void generateProgram(const AstNode &decoratedAst, const SymbolTable &symbols,
                     CodeGenContext &context);
void generateStatement(const AstNode &node, const SymbolTable &symbols,
                       CodeGenContext &context);
void generateExpression(const AstNode &node, const SymbolTable &symbols,
                        CodeGenContext &context);
void generateCall(const AstNode &node, const SymbolTable &symbols,
                  CodeGenContext &context, bool asExpression);

#endif // CODE_GENERATOR_H
