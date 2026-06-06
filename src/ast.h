#ifndef AST_H
#define AST_H

#include <string>
#include <utility>
#include <vector>

enum class AstKind {
    Program,
    Declarations,
    ConstDecl,
    TypeDecl,
    VarDecl,
    ProcedureDecl,
    FunctionDecl,
    Parameters,
    ParameterDecl,
    NamedType,
    ArrayType,
    RangeType,
    EnumType,
    RecordType,
    FieldDecl,
    CompoundStmt,
    StatementList,
    EmptyStmt,
    AssignStmt,
    IfStmt,
    CaseStmt,
    CaseBranch,
    CaseLabels,
    WhileStmt,
    RepeatStmt,
    ForStmt,
    Call,
    Variable,
    IndexAccess,
    FieldAccess,
    BinaryExpr,
    UnaryExpr,
    Literal,
    Identifier
};

struct AstNode {
    AstKind kind;
    std::string text;
    std::vector<AstNode> children;

    // Diisi pada semantic analysis/decorated AST.
    std::string inferredType;
    int symbolIndex;
    int lexicalLevel;

    explicit AstNode(AstKind nodeKind, std::string nodeText = "")
        : kind(nodeKind), text(std::move(nodeText)), symbolIndex(-1),
          lexicalLevel(-1) {}

    AstNode &addChild(AstNode child) {
        children.push_back(std::move(child));
        return children.back();
    }
};

std::string astKindToString(AstKind kind);
AstKind astKindFromString(const std::string &name);
std::string renderAst(const AstNode &root);
AstNode parseRenderedAst(const std::string &text);

#endif // AST_H
