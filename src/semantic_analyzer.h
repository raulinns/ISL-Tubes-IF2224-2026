#ifndef SEMANTIC_ANALYZER_H
#define SEMANTIC_ANALYZER_H

#include "ast.h"
#include "symbol_table.h"

#include <string>
#include <vector>

struct SemanticDiagnostic {
    enum class Severity { Error, Warning };

    Severity severity;
    std::string message;
};

struct SemanticResult {
    AstNode ast;
    SymbolTable symbols;
    std::vector<SemanticDiagnostic> diagnostics;

    bool ok() const;
};

SemanticResult analyzeSemantics(AstNode ast);

#endif // SEMANTIC_ANALYZER_H
