#include "frontend_artifact.h"

#include <sstream>
#include <stdexcept>

namespace {

const char *kAstHeader = "----- Decorated AST -----";
const char *kSymbolsHeader = "----- Symbol Table -----";
const char *kDiagnosticsHeader = "----- Semantic Diagnostics -----";

std::string trim(const std::string &text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string section(const std::string &text, const std::string &header,
                    const std::string &nextHeader) {
    const std::size_t start = text.find(header);
    if (start == std::string::npos) {
        throw std::runtime_error("Missing artifact section: " + header);
    }
    std::size_t body = start + header.size();
    while (body < text.size() && (text[body] == '\r' || text[body] == '\n')) {
        ++body;
    }
    std::size_t finish = text.size();
    if (!nextHeader.empty()) {
        finish = text.find(nextHeader, body);
        if (finish == std::string::npos) {
            throw std::runtime_error("Missing artifact section: " + nextHeader);
        }
    } else {
        const std::size_t laterSection = text.find("\n----- ", body);
        if (laterSection != std::string::npos) {
            finish = laterSection;
        }
    }
    return text.substr(body, finish - body);
}

std::vector<SemanticDiagnostic> parseDiagnostics(const std::string &text) {
    std::vector<SemanticDiagnostic> diagnostics;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line == "No semantic diagnostics.") {
            continue;
        }
        if (line.rfind("error:", 0) == 0) {
            diagnostics.push_back(
                {SemanticDiagnostic::Severity::Error, trim(line.substr(6))});
        } else if (line.rfind("warning:", 0) == 0) {
            diagnostics.push_back(
                {SemanticDiagnostic::Severity::Warning, trim(line.substr(8))});
        } else {
            throw std::runtime_error("Invalid semantic diagnostic line: " + line);
        }
    }
    return diagnostics;
}

void validateNode(const AstNode &node, const SymbolTable &symbols) {
    if (node.symbolIndex < -1) {
        throw std::runtime_error("AST contains a negative symbol index");
    }
    if (node.symbolIndex >= static_cast<int>(symbols.tab().size())) {
        throw std::runtime_error("AST symbol index is outside tab: " +
                                 std::to_string(node.symbolIndex));
    }
    if (node.symbolIndex >= 0) {
        const TabEntry &entry = symbols.tabEntry(node.symbolIndex);
        if (!node.text.empty() &&
            (node.kind == AstKind::Identifier ||
             node.kind == AstKind::Variable ||
             node.kind == AstKind::VarDecl ||
             node.kind == AstKind::ParameterDecl ||
             node.kind == AstKind::ProcedureDecl ||
             node.kind == AstKind::FunctionDecl) &&
            SymbolTable::normalizeIdentifier(node.text) != entry.identifier) {
            throw std::runtime_error("AST symbol reference does not match identifier: " +
                                     node.text);
        }
    }
    if (node.lexicalLevel < -1) {
        throw std::runtime_error("AST contains an invalid lexical level");
    }
    if (!node.inferredType.empty()) {
        typeKindFromString(node.inferredType);
    }
    switch (node.kind) {
    case AstKind::Program:
        if (node.children.size() < 2) {
            throw std::runtime_error(
                "Program AST requires declarations and body");
        }
        break;
    case AstKind::AssignStmt:
    case AstKind::BinaryExpr:
        if (node.children.size() != 2) {
            throw std::runtime_error(astKindToString(node.kind) +
                                     " requires two children");
        }
        break;
    case AstKind::UnaryExpr:
        if (node.children.size() != 1) {
            throw std::runtime_error("UnaryExpr requires one child");
        }
        break;
    case AstKind::IfStmt:
        if (node.children.size() < 2 || node.children.size() > 3) {
            throw std::runtime_error("IfStmt has invalid child count");
        }
        break;
    case AstKind::WhileStmt:
    case AstKind::RepeatStmt:
        if (node.children.size() != 2) {
            throw std::runtime_error(astKindToString(node.kind) +
                                     " requires two children");
        }
        break;
    case AstKind::ForStmt:
        if (node.children.size() != 4) {
            throw std::runtime_error("ForStmt requires four children");
        }
        break;
    default:
        break;
    }
    for (const AstNode &child : node.children) {
        validateNode(child, symbols);
    }
}

} // namespace

FrontendArtifact::FrontendArtifact() : ast(AstKind::Program) {}

bool FrontendArtifact::semanticOk() const {
    for (const SemanticDiagnostic &diagnostic : diagnostics) {
        if (diagnostic.severity == SemanticDiagnostic::Severity::Error) {
            return false;
        }
    }
    return true;
}

FrontendArtifact parseFrontendArtifact(const std::string &text) {
    FrontendArtifact artifact;
    artifact.ast = parseRenderedAst(section(text, kAstHeader, kSymbolsHeader));
    artifact.symbols.loadRenderedAll(
        section(text, kSymbolsHeader, kDiagnosticsHeader));
    artifact.diagnostics =
        parseDiagnostics(section(text, kDiagnosticsHeader, ""));
    validateFrontendArtifact(artifact);
    return artifact;
}

std::string renderFrontendArtifact(const FrontendArtifact &artifact) {
    std::ostringstream out;
    out << kAstHeader << "\n\n" << renderAst(artifact.ast);
    out << "\n" << kSymbolsHeader << "\n\n" << artifact.symbols.renderAll();
    out << "\n" << kDiagnosticsHeader << "\n\n";
    if (artifact.diagnostics.empty()) {
        out << "No semantic diagnostics.\n";
    } else {
        for (const SemanticDiagnostic &diagnostic : artifact.diagnostics) {
            out << (diagnostic.severity == SemanticDiagnostic::Severity::Error
                        ? "error: "
                        : "warning: ")
                << diagnostic.message << '\n';
        }
    }
    return out.str();
}

void validateFrontendArtifact(const FrontendArtifact &artifact) {
    if (artifact.ast.kind != AstKind::Program) {
        throw std::runtime_error("Decorated AST root must be Program");
    }
    if (artifact.symbols.tab().empty() || artifact.symbols.btab().empty()) {
        throw std::runtime_error("Artifact symbol tables cannot be empty");
    }
    for (const TabEntry &entry : artifact.symbols.tab()) {
        if (entry.lev < 0) {
            throw std::runtime_error("Negative symbol lexical level");
        }
        if (entry.link < 0 ||
            entry.link >= static_cast<int>(artifact.symbols.tab().size())) {
            throw std::runtime_error("Invalid tab link for " + entry.identifier);
        }
        if (entry.type == TypeKind::Array &&
            (entry.typeRef < 0 ||
             entry.typeRef >=
                 static_cast<int>(artifact.symbols.atab().size()))) {
            throw std::runtime_error("Invalid array reference for " +
                                     entry.identifier);
        }
        if (entry.type == TypeKind::Record &&
            (entry.typeRef < 0 ||
             entry.typeRef >=
                 static_cast<int>(artifact.symbols.btab().size()))) {
            throw std::runtime_error("Invalid record reference for " +
                                     entry.identifier);
        }
        if ((entry.obj == ObjectKind::Procedure ||
             entry.obj == ObjectKind::Function) &&
            entry.ref != 0 &&
            (entry.ref < 0 ||
             entry.ref >= static_cast<int>(artifact.symbols.btab().size()))) {
            throw std::runtime_error("Invalid block reference for " +
                                     entry.identifier);
        }
    }
    validateNode(artifact.ast, artifact.symbols);
}
