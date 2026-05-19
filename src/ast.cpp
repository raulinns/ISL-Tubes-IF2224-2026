#include "ast.h"

#include <sstream>

namespace {

std::string makeNodeLabel(const AstNode &node) {
    std::string label = astKindToString(node.kind);
    if (!node.text.empty()) {
        label += "(" + node.text + ")";
    }

    std::vector<std::string> annotations;
    if (!node.inferredType.empty()) {
        annotations.push_back("type=" + node.inferredType);
    }
    if (node.symbolIndex >= 0) {
        annotations.push_back("sym=" + std::to_string(node.symbolIndex));
    }
    if (node.lexicalLevel >= 0) {
        annotations.push_back("level=" + std::to_string(node.lexicalLevel));
    }

    if (!annotations.empty()) {
        label += " [";
        for (std::size_t i = 0; i < annotations.size(); ++i) {
            if (i > 0) {
                label += ", ";
            }
            label += annotations[i];
        }
        label += "]";
    }

    return label;
}

void renderBranch(const AstNode &node, const std::string &prefix, bool isLast,
                  std::ostringstream &out) {
    out << prefix << (isLast ? "\\-- " : "|-- ") << makeNodeLabel(node)
        << "\n";

    const std::string nextPrefix = prefix + (isLast ? "    " : "|   ");
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        renderBranch(node.children[i], nextPrefix,
                     i + 1 == node.children.size(), out);
    }
}

} // namespace

std::string astKindToString(AstKind kind) {
    switch (kind) {
    case AstKind::Program:
        return "Program";
    case AstKind::Declarations:
        return "Declarations";
    case AstKind::ConstDecl:
        return "ConstDecl";
    case AstKind::TypeDecl:
        return "TypeDecl";
    case AstKind::VarDecl:
        return "VarDecl";
    case AstKind::ProcedureDecl:
        return "ProcedureDecl";
    case AstKind::FunctionDecl:
        return "FunctionDecl";
    case AstKind::Parameters:
        return "Parameters";
    case AstKind::ParameterDecl:
        return "ParameterDecl";
    case AstKind::NamedType:
        return "NamedType";
    case AstKind::ArrayType:
        return "ArrayType";
    case AstKind::RangeType:
        return "RangeType";
    case AstKind::EnumType:
        return "EnumType";
    case AstKind::RecordType:
        return "RecordType";
    case AstKind::FieldDecl:
        return "FieldDecl";
    case AstKind::CompoundStmt:
        return "CompoundStmt";
    case AstKind::StatementList:
        return "StatementList";
    case AstKind::EmptyStmt:
        return "EmptyStmt";
    case AstKind::AssignStmt:
        return "AssignStmt";
    case AstKind::IfStmt:
        return "IfStmt";
    case AstKind::CaseStmt:
        return "CaseStmt";
    case AstKind::CaseBranch:
        return "CaseBranch";
    case AstKind::CaseLabels:
        return "CaseLabels";
    case AstKind::WhileStmt:
        return "WhileStmt";
    case AstKind::RepeatStmt:
        return "RepeatStmt";
    case AstKind::ForStmt:
        return "ForStmt";
    case AstKind::Call:
        return "Call";
    case AstKind::Variable:
        return "Variable";
    case AstKind::IndexAccess:
        return "IndexAccess";
    case AstKind::FieldAccess:
        return "FieldAccess";
    case AstKind::BinaryExpr:
        return "BinaryExpr";
    case AstKind::UnaryExpr:
        return "UnaryExpr";
    case AstKind::Literal:
        return "Literal";
    case AstKind::Identifier:
        return "Identifier";
    }

    return "UnknownAstKind";
}

std::string renderAst(const AstNode &root) {
    std::ostringstream out;
    out << makeNodeLabel(root) << "\n";

    for (std::size_t i = 0; i < root.children.size(); ++i) {
        renderBranch(root.children[i], "", i + 1 == root.children.size(), out);
    }

    return out.str();
}
