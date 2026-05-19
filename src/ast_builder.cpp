#include "ast_builder.h"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct LabelParts {
    std::string name;
    std::string value;
    bool hasValue;
};

LabelParts parseLabel(const std::string &label) {
    const std::size_t open = label.find('(');
    const std::size_t close = label.rfind(')');

    if (open == std::string::npos || close == std::string::npos ||
        close <= open) {
        return {label, "", false};
    }

    return {label.substr(0, open), label.substr(open + 1, close - open - 1),
            true};
}

bool hasExactLabel(const ParseNode &node, const std::string &label) {
    return node.label == label;
}

bool hasTokenName(const ParseNode &node, const std::string &name) {
    return parseLabel(node.label).name == name;
}

const ParseNode *findFirstChild(const ParseNode &node, const std::string &label) {
    for (const ParseNode &child : node.children) {
        if (child.label == label) {
            return &child;
        }
    }
    return nullptr;
}

std::string expectTokenValue(const ParseNode &node, const std::string &tokenName) {
    const LabelParts parts = parseLabel(node.label);
    if (parts.name != tokenName || !parts.hasValue) {
        throw std::runtime_error("AST builder expected token " + tokenName +
                                 " but got " + node.label);
    }
    return parts.value;
}

AstNode buildExpression(const ParseNode &node);
AstNode buildSimpleExpression(const ParseNode &node);
AstNode buildTerm(const ParseNode &node);
AstNode buildFactor(const ParseNode &node);
AstNode buildStatement(const ParseNode &node);
AstNode buildStatementListNode(const ParseNode &node);
AstNode buildCompoundStatement(const ParseNode &node);
AstNode buildVariable(const ParseNode &node);
AstNode buildProcedureOrFunctionCall(const ParseNode &node);
AstNode buildTypeNode(const ParseNode &node);
AstNode buildConstant(const ParseNode &node);

AstNode buildAtomicValueNode(const ParseNode &node) {
    const LabelParts parts = parseLabel(node.label);

    if (parts.name == "ident" && parts.hasValue) {
        return AstNode(AstKind::Identifier, parts.value);
    }
    if ((parts.name == "intcon" || parts.name == "realcon" ||
         parts.name == "charcon" || parts.name == "string") &&
        parts.hasValue) {
        return AstNode(AstKind::Literal, parts.value);
    }

    throw std::runtime_error("AST builder cannot convert atomic token " +
                             node.label);
}

std::vector<std::string> collectIdentifierList(const ParseNode &node) {
    if (!hasExactLabel(node, "<identifier-list>")) {
        throw std::runtime_error("AST builder expected <identifier-list> but got " +
                                 node.label);
    }

    std::vector<std::string> names;
    for (const ParseNode &child : node.children) {
        if (hasTokenName(child, "ident")) {
            names.push_back(expectTokenValue(child, "ident"));
        }
    }
    return names;
}

AstNode buildParameters(const ParseNode *nodePtr) {
    AstNode params(AstKind::Parameters);
    if (nodePtr == nullptr) {
        return params;
    }

    const ParseNode &node = *nodePtr;
    if (!hasExactLabel(node, "<formal-parameter-list>")) {
        throw std::runtime_error(
            "AST builder expected <formal-parameter-list> but got " + node.label);
    }

    for (const ParseNode &child : node.children) {
        if (!hasExactLabel(child, "<parameter-group>")) {
            continue;
        }

        const std::vector<std::string> names =
            collectIdentifierList(child.children.at(0));
        AstNode typeNode = hasExactLabel(child.children.at(2), "<array-type>")
                               ? buildTypeNode(child.children.at(2))
                               : AstNode(AstKind::NamedType,
                                         expectTokenValue(child.children.at(2),
                                                          "ident"));

        for (const std::string &name : names) {
            AstNode param(AstKind::ParameterDecl, name);
            param.addChild(typeNode);
            params.addChild(param);
        }
    }

    return params;
}

void appendDeclarations(const ParseNode &node, AstNode &declarations) {
    if (hasExactLabel(node, "<const-declaration>")) {
        for (std::size_t i = 1; i + 2 < node.children.size(); i += 4) {
            AstNode decl(AstKind::ConstDecl,
                         expectTokenValue(node.children[i], "ident"));
            decl.addChild(buildConstant(node.children[i + 2]));
            declarations.addChild(decl);
        }
        return;
    }

    if (hasExactLabel(node, "<type-declaration>")) {
        for (std::size_t i = 1; i + 2 < node.children.size(); i += 4) {
            AstNode decl(AstKind::TypeDecl,
                         expectTokenValue(node.children[i], "ident"));
            decl.addChild(buildTypeNode(node.children[i + 2]));
            declarations.addChild(decl);
        }
        return;
    }

    if (hasExactLabel(node, "<var-declaration>")) {
        for (std::size_t i = 1; i + 2 < node.children.size(); i += 4) {
            const std::vector<std::string> names =
                collectIdentifierList(node.children[i]);
            AstNode typeNode = buildTypeNode(node.children[i + 2]);

            for (const std::string &name : names) {
                AstNode decl(AstKind::VarDecl, name);
                decl.addChild(typeNode);
                declarations.addChild(decl);
            }
        }
        return;
    }

    if (hasExactLabel(node, "<subprogram-declaration>")) {
        if (node.children.empty()) {
            throw std::runtime_error(
                "AST builder found empty <subprogram-declaration>");
        }

        const ParseNode &subprogram = node.children.front();

        if (hasExactLabel(subprogram, "<procedure-declaration>")) {
            AstNode decl(AstKind::ProcedureDecl,
                         expectTokenValue(subprogram.children.at(1), "ident"));

            std::size_t blockIndex = 3;
            if (hasExactLabel(subprogram.children.at(2), "<formal-parameter-list>")) {
                decl.addChild(buildParameters(&subprogram.children.at(2)));
                blockIndex = 4;
            } else {
                decl.addChild(buildParameters(nullptr));
            }

            const ParseNode &block = subprogram.children.at(blockIndex);
            decl.addChild(AstNode(AstKind::Declarations));
            appendDeclarations(block.children.at(0), decl.children.back());
            decl.addChild(buildCompoundStatement(block.children.at(1)));

            declarations.addChild(decl);
            return;
        }

        if (hasExactLabel(subprogram, "<function-declaration>")) {
            AstNode decl(AstKind::FunctionDecl,
                         expectTokenValue(subprogram.children.at(1), "ident"));

            std::size_t returnTypeIndex = 3;
            std::size_t blockIndex = 5;
            if (hasExactLabel(subprogram.children.at(2), "<formal-parameter-list>")) {
                decl.addChild(buildParameters(&subprogram.children.at(2)));
                returnTypeIndex = 4;
                blockIndex = 6;
            } else {
                decl.addChild(buildParameters(nullptr));
            }

            decl.addChild(AstNode(AstKind::NamedType, expectTokenValue(
                                                         subprogram.children.at(returnTypeIndex),
                                                         "ident")));

            const ParseNode &block = subprogram.children.at(blockIndex);
            decl.addChild(AstNode(AstKind::Declarations));
            appendDeclarations(block.children.at(0), decl.children.back());
            decl.addChild(buildCompoundStatement(block.children.at(1)));

            declarations.addChild(decl);
            return;
        }
    }

    if (hasExactLabel(node, "<declaration-part>")) {
        for (const ParseNode &child : node.children) {
            appendDeclarations(child, declarations);
        }
        return;
    }

    throw std::runtime_error("AST builder cannot convert declaration node " +
                             node.label);
}

AstNode buildDeclarationPart(const ParseNode &node) {
    AstNode declarations(AstKind::Declarations);
    appendDeclarations(node, declarations);
    return declarations;
}

void appendRecordFields(const ParseNode &fieldListNode, AstNode &recordNode) {
    if (!hasExactLabel(fieldListNode, "<field-list>")) {
        throw std::runtime_error("AST builder expected <field-list> but got " +
                                 fieldListNode.label);
    }

    for (const ParseNode &child : fieldListNode.children) {
        if (!hasExactLabel(child, "<field-part>")) {
            continue;
        }

        const std::vector<std::string> names =
            collectIdentifierList(child.children.at(0));
        AstNode typeNode = buildTypeNode(child.children.at(2));

        for (const std::string &name : names) {
            AstNode field(AstKind::FieldDecl, name);
            field.addChild(typeNode);
            recordNode.addChild(field);
        }
    }
}

AstNode buildTypeNode(const ParseNode &node) {
    if (hasExactLabel(node, "<type>")) {
        if (node.children.empty()) {
            throw std::runtime_error("AST builder found empty <type>");
        }
        return buildTypeNode(node.children.front());
    }

    if (hasExactLabel(node, "<array-type>")) {
        AstNode typeNode(AstKind::ArrayType);
        typeNode.addChild(hasExactLabel(node.children.at(2), "<range>")
                              ? buildTypeNode(node.children.at(2))
                              : AstNode(AstKind::NamedType, expectTokenValue(
                                                             node.children.at(2),
                                                             "ident")));
        typeNode.addChild(buildTypeNode(node.children.at(5)));
        return typeNode;
    }

    if (hasExactLabel(node, "<range>")) {
        AstNode typeNode(AstKind::RangeType);
        typeNode.addChild(buildConstant(node.children.at(0)));
        typeNode.addChild(buildConstant(node.children.at(3)));
        return typeNode;
    }

    if (hasExactLabel(node, "<enumerated>")) {
        AstNode typeNode(AstKind::EnumType);
        for (const ParseNode &child : node.children) {
            if (hasTokenName(child, "ident")) {
                typeNode.addChild(
                    AstNode(AstKind::Identifier, expectTokenValue(child, "ident")));
            }
        }
        return typeNode;
    }

    if (hasExactLabel(node, "<record-type>")) {
        AstNode typeNode(AstKind::RecordType);
        appendRecordFields(node.children.at(1), typeNode);
        return typeNode;
    }

    if (hasTokenName(node, "ident")) {
        return AstNode(AstKind::NamedType, expectTokenValue(node, "ident"));
    }

    throw std::runtime_error("AST builder cannot convert type node " +
                             node.label);
}

AstNode buildConstant(const ParseNode &node) {
    if (!hasExactLabel(node, "<constant>")) {
        throw std::runtime_error("AST builder expected <constant> but got " +
                                 node.label);
    }

    if (node.children.size() == 1) {
        return buildAtomicValueNode(node.children.front());
    }

    if (node.children.size() == 2) {
        AstNode unary(AstKind::UnaryExpr, parseLabel(node.children.front().label).name);
        unary.addChild(buildAtomicValueNode(node.children.back()));
        return unary;
    }

    throw std::runtime_error("AST builder found unsupported constant form");
}

AstNode buildVariable(const ParseNode &node) {
    if (!hasExactLabel(node, "<variable>")) {
        throw std::runtime_error("AST builder expected <variable> but got " +
                                 node.label);
    }

    AstNode variable(AstKind::Variable,
                     expectTokenValue(node.children.front(), "ident"));

    for (std::size_t i = 1; i < node.children.size(); ++i) {
        const ParseNode &component = node.children[i];
        if (!hasExactLabel(component, "<component-variable>")) {
            continue;
        }

        if (hasTokenName(component.children.front(), "lbrack")) {
            AstNode access(AstKind::IndexAccess);
            const ParseNode &indexList = component.children.at(1);
            for (const ParseNode &indexNode : indexList.children) {
                if (hasTokenName(indexNode, "comma")) {
                    continue;
                }
                access.addChild(buildAtomicValueNode(indexNode));
            }
            variable.addChild(access);
        } else {
            variable.addChild(AstNode(
                AstKind::FieldAccess,
                expectTokenValue(component.children.at(1), "ident")));
        }
    }

    return variable;
}

AstNode buildProcedureOrFunctionCall(const ParseNode &node) {
    if (!hasExactLabel(node, "<procedure/function-call>")) {
        throw std::runtime_error(
            "AST builder expected <procedure/function-call> but got " + node.label);
    }

    AstNode call(AstKind::Call, expectTokenValue(node.children.front(), "ident"));

    const ParseNode *paramList = findFirstChild(node, "<parameter-list>");
    if (paramList == nullptr) {
        return call;
    }

    for (const ParseNode &child : paramList->children) {
        if (hasExactLabel(child, "<expression>")) {
            call.addChild(buildExpression(child));
        }
    }

    return call;
}

AstNode buildFactor(const ParseNode &node) {
    if (!hasExactLabel(node, "<factor>")) {
        throw std::runtime_error("AST builder expected <factor> but got " +
                                 node.label);
    }

    if (node.children.empty()) {
        throw std::runtime_error("AST builder found empty <factor>");
    }

    const ParseNode &first = node.children.front();

    if (hasTokenName(first, "intcon") || hasTokenName(first, "realcon") ||
        hasTokenName(first, "charcon") || hasTokenName(first, "string")) {
        return buildAtomicValueNode(first);
    }

    if (hasTokenName(first, "lparent")) {
        return buildExpression(node.children.at(1));
    }

    if (hasTokenName(first, "notsy")) {
        AstNode unary(AstKind::UnaryExpr, "notsy");
        unary.addChild(buildFactor(node.children.at(1)));
        return unary;
    }

    if (hasExactLabel(first, "<procedure/function-call>")) {
        return buildProcedureOrFunctionCall(first);
    }

    if (hasExactLabel(first, "<variable>")) {
        return buildVariable(first);
    }

    if (hasTokenName(first, "ident")) {
        return AstNode(AstKind::Identifier, expectTokenValue(first, "ident"));
    }

    throw std::runtime_error("AST builder cannot convert factor " + node.label);
}

AstNode buildTerm(const ParseNode &node) {
    if (!hasExactLabel(node, "<term>")) {
        throw std::runtime_error("AST builder expected <term> but got " +
                                 node.label);
    }

    AstNode expr = buildFactor(node.children.at(0));

    for (std::size_t i = 1; i + 1 < node.children.size(); i += 2) {
        const std::string op = parseLabel(node.children.at(i).children.front().label).name;
        AstNode bin(AstKind::BinaryExpr, op);
        bin.addChild(expr);
        bin.addChild(buildFactor(node.children.at(i + 1)));
        expr = std::move(bin);
    }

    return expr;
}

AstNode buildSimpleExpression(const ParseNode &node) {
    if (!hasExactLabel(node, "<simple-expression>")) {
        throw std::runtime_error(
            "AST builder expected <simple-expression> but got " + node.label);
    }

    std::size_t index = 0;
    if (hasTokenName(node.children.front(), "plus") ||
        hasTokenName(node.children.front(), "minus")) {
        AstNode unary(AstKind::UnaryExpr,
                      parseLabel(node.children.front().label).name);
        unary.addChild(buildTerm(node.children.at(1)));
        AstNode expr = std::move(unary);
        index = 2;
        for (; index + 1 < node.children.size(); index += 2) {
            const std::string op =
                parseLabel(node.children.at(index).children.front().label).name;
            AstNode bin(AstKind::BinaryExpr, op);
            bin.addChild(expr);
            bin.addChild(buildTerm(node.children.at(index + 1)));
            expr = std::move(bin);
        }
        return expr;
    }

    AstNode expr = buildTerm(node.children.at(0));
    index = 1;

    for (; index + 1 < node.children.size(); index += 2) {
        const std::string op =
            parseLabel(node.children.at(index).children.front().label).name;
        AstNode bin(AstKind::BinaryExpr, op);
        bin.addChild(expr);
        bin.addChild(buildTerm(node.children.at(index + 1)));
        expr = std::move(bin);
    }

    return expr;
}

AstNode buildExpression(const ParseNode &node) {
    if (!hasExactLabel(node, "<expression>")) {
        throw std::runtime_error("AST builder expected <expression> but got " +
                                 node.label);
    }

    AstNode expr = buildSimpleExpression(node.children.at(0));

    if (node.children.size() == 3) {
        const std::string op =
            parseLabel(node.children.at(1).children.front().label).name;
        AstNode bin(AstKind::BinaryExpr, op);
        bin.addChild(expr);
        bin.addChild(buildSimpleExpression(node.children.at(2)));
        return bin;
    }

    return expr;
}

void appendCaseBranches(const ParseNode &node, AstNode &caseStmt) {
    if (!hasExactLabel(node, "<case-block>")) {
        throw std::runtime_error("AST builder expected <case-block> but got " +
                                 node.label);
    }

    AstNode branch(AstKind::CaseBranch);
    AstNode labels(AstKind::CaseLabels);

    std::size_t index = 0;
    while (index < node.children.size() && hasExactLabel(node.children[index], "<constant>")) {
        labels.addChild(buildConstant(node.children[index]));
        ++index;
        if (index < node.children.size() && hasTokenName(node.children[index], "comma")) {
            ++index;
        }
    }

    branch.addChild(labels);

    while (index < node.children.size() && !hasExactLabel(node.children[index], "<statement>")) {
        ++index;
    }
    if (index >= node.children.size()) {
        throw std::runtime_error("AST builder found case branch without statement");
    }

    branch.addChild(buildStatement(node.children[index]));
    caseStmt.addChild(branch);

    for (++index; index < node.children.size(); ++index) {
        if (hasExactLabel(node.children[index], "<case-block>")) {
            appendCaseBranches(node.children[index], caseStmt);
        }
    }
}

AstNode buildStatement(const ParseNode &node) {
    if (!hasExactLabel(node, "<statement>")) {
        throw std::runtime_error("AST builder expected <statement> but got " +
                                 node.label);
    }

    if (node.children.empty()) {
        return AstNode(AstKind::EmptyStmt);
    }

    const ParseNode &child = node.children.front();

    if (hasExactLabel(child, "<assignment-statement>")) {
        AstNode stmt(AstKind::AssignStmt);
        stmt.addChild(buildVariable(child.children.at(0)));
        stmt.addChild(buildExpression(child.children.at(2)));
        return stmt;
    }

    if (hasExactLabel(child, "<compound-statement>")) {
        return buildCompoundStatement(child);
    }

    if (hasExactLabel(child, "<if-statement>")) {
        AstNode stmt(AstKind::IfStmt);
        stmt.addChild(buildExpression(child.children.at(1)));
        stmt.addChild(buildStatement(child.children.at(3)));
        if (child.children.size() > 4) {
            stmt.addChild(buildStatement(child.children.at(5)));
        }
        return stmt;
    }

    if (hasExactLabel(child, "<case-statement>")) {
        AstNode stmt(AstKind::CaseStmt);
        stmt.addChild(buildExpression(child.children.at(1)));
        appendCaseBranches(child.children.at(3), stmt);
        return stmt;
    }

    if (hasExactLabel(child, "<while-statement>")) {
        AstNode stmt(AstKind::WhileStmt);
        stmt.addChild(buildExpression(child.children.at(1)));
        stmt.addChild(buildCompoundStatement(child.children.at(3)));
        return stmt;
    }

    if (hasExactLabel(child, "<repeat-statement>")) {
        AstNode stmt(AstKind::RepeatStmt);
        stmt.addChild(buildStatementListNode(child.children.at(1)));
        stmt.addChild(buildExpression(child.children.at(3)));
        return stmt;
    }

    if (hasExactLabel(child, "<for-statement>")) {
        AstNode stmt(
            AstKind::ForStmt,
            parseLabel(child.children.at(4).label).name == "tosy" ? "to" : "downto");
        stmt.addChild(
            AstNode(AstKind::Identifier, expectTokenValue(child.children.at(1), "ident")));
        stmt.addChild(buildExpression(child.children.at(3)));
        stmt.addChild(buildExpression(child.children.at(5)));
        stmt.addChild(buildCompoundStatement(child.children.at(7)));
        return stmt;
    }

    if (hasExactLabel(child, "<procedure/function-call>")) {
        return buildProcedureOrFunctionCall(child);
    }

    if (hasTokenName(child, "ident")) {
        return AstNode(AstKind::Identifier, expectTokenValue(child, "ident"));
    }

    throw std::runtime_error("AST builder cannot convert statement " + child.label);
}

AstNode buildStatementListNode(const ParseNode &node) {
    if (!hasExactLabel(node, "<statement-list>")) {
        throw std::runtime_error(
            "AST builder expected <statement-list> but got " + node.label);
    }

    AstNode list(AstKind::StatementList);
    for (const ParseNode &child : node.children) {
        if (hasExactLabel(child, "<statement>")) {
            list.addChild(buildStatement(child));
        }
    }
    return list;
}

AstNode buildCompoundStatement(const ParseNode &node) {
    if (!hasExactLabel(node, "<compound-statement>")) {
        throw std::runtime_error(
            "AST builder expected <compound-statement> but got " + node.label);
    }

    AstNode compound(AstKind::CompoundStmt);
    const ParseNode *statementList = findFirstChild(node, "<statement-list>");
    if (statementList != nullptr) {
        for (const AstNode &stmt : buildStatementListNode(*statementList).children) {
            compound.addChild(stmt);
        }
    }

    return compound;
}

AstNode buildProgram(const ParseNode &node) {
    if (!hasExactLabel(node, "<program>")) {
        throw std::runtime_error("AST builder expected <program> but got " +
                                 node.label);
    }

    const ParseNode &header = node.children.at(0);
    AstNode program(AstKind::Program,
                    expectTokenValue(header.children.at(1), "ident"));
    program.addChild(buildDeclarationPart(node.children.at(1)));
    program.addChild(buildCompoundStatement(node.children.at(2)));
    return program;
}

} // namespace

AstNode buildAstFromParseTree(const ParseNode &parseTree) {
    return buildProgram(parseTree);
}
