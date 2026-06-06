#ifndef FRONTEND_ARTIFACT_H
#define FRONTEND_ARTIFACT_H

#include "ast.h"
#include "semantic_analyzer.h"
#include "symbol_table.h"

#include <string>
#include <vector>

struct FrontendArtifact {
    AstNode ast;
    SymbolTable symbols;
    std::vector<SemanticDiagnostic> diagnostics;

    FrontendArtifact();
    bool semanticOk() const;
};

FrontendArtifact parseFrontendArtifact(const std::string &text);
std::string renderFrontendArtifact(const FrontendArtifact &artifact);
void validateFrontendArtifact(const FrontendArtifact &artifact);

#endif // FRONTEND_ARTIFACT_H
