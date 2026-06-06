#include "ast.h"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

std::string trim(const std::string &text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

struct ParsedAstLabel {
    AstKind kind;
    std::string text;
    std::string inferredType;
    int symbolIndex = -1;
    int lexicalLevel = -1;
};

AstNode *nodeAtPath(AstNode &root, const std::vector<std::size_t> &path) {
    AstNode *node = &root;
    for (const std::size_t index : path) {
        if (index >= node->children.size()) {
            throw std::runtime_error("Invalid decorated AST indentation");
        }
        node = &node->children[index];
    }
    return node;
}

ParsedAstLabel parseAstLabel(const std::string &rawLabel) {
    std::string label = trim(rawLabel);
    std::string annotationsText;

    const std::size_t annotationStart = label.rfind(" [");
    if (annotationStart != std::string::npos && !label.empty() &&
        label.back() == ']') {
        annotationsText =
            label.substr(annotationStart + 2, label.size() - annotationStart - 3);
        label = label.substr(0, annotationStart);
    }

    std::string kindName = label;
    std::string nodeText;
    const std::size_t open = label.find('(');
    const std::size_t close = label.rfind(')');
    if (open != std::string::npos && close != std::string::npos && close > open) {
        kindName = label.substr(0, open);
        nodeText = label.substr(open + 1, close - open - 1);
    }

    ParsedAstLabel parsed{astKindFromString(trim(kindName)), nodeText, "", -1, -1};

    if (!annotationsText.empty()) {
        std::size_t start = 0;
        while (start < annotationsText.size()) {
            std::size_t end = annotationsText.find(", ", start);
            if (end == std::string::npos) {
                end = annotationsText.size();
            }

            const std::string item = annotationsText.substr(start, end - start);
            const std::size_t equals = item.find('=');
            if (equals != std::string::npos) {
                const std::string key = trim(item.substr(0, equals));
                const std::string value = trim(item.substr(equals + 1));
                if (key == "type") {
                    parsed.inferredType = value;
                } else if (key == "sym") {
                    parsed.symbolIndex = std::stoi(value);
                } else if (key == "level") {
                    parsed.lexicalLevel = std::stoi(value);
                }
            }

            start = end + 2;
        }
    }

    return parsed;
}

AstNode makeNodeFromLabel(const std::string &label) {
    const ParsedAstLabel parsed = parseAstLabel(label);
    AstNode node(parsed.kind, parsed.text);
    node.inferredType = parsed.inferredType;
    node.symbolIndex = parsed.symbolIndex;
    node.lexicalLevel = parsed.lexicalLevel;
    return node;
}

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

AstKind astKindFromString(const std::string &name) {
    static const std::unordered_map<std::string, AstKind> kKinds = {
        {"Program", AstKind::Program},
        {"Declarations", AstKind::Declarations},
        {"ConstDecl", AstKind::ConstDecl},
        {"TypeDecl", AstKind::TypeDecl},
        {"VarDecl", AstKind::VarDecl},
        {"ProcedureDecl", AstKind::ProcedureDecl},
        {"FunctionDecl", AstKind::FunctionDecl},
        {"Parameters", AstKind::Parameters},
        {"ParameterDecl", AstKind::ParameterDecl},
        {"NamedType", AstKind::NamedType},
        {"ArrayType", AstKind::ArrayType},
        {"RangeType", AstKind::RangeType},
        {"EnumType", AstKind::EnumType},
        {"RecordType", AstKind::RecordType},
        {"FieldDecl", AstKind::FieldDecl},
        {"CompoundStmt", AstKind::CompoundStmt},
        {"StatementList", AstKind::StatementList},
        {"EmptyStmt", AstKind::EmptyStmt},
        {"AssignStmt", AstKind::AssignStmt},
        {"IfStmt", AstKind::IfStmt},
        {"CaseStmt", AstKind::CaseStmt},
        {"CaseBranch", AstKind::CaseBranch},
        {"CaseLabels", AstKind::CaseLabels},
        {"WhileStmt", AstKind::WhileStmt},
        {"RepeatStmt", AstKind::RepeatStmt},
        {"ForStmt", AstKind::ForStmt},
        {"Call", AstKind::Call},
        {"Variable", AstKind::Variable},
        {"IndexAccess", AstKind::IndexAccess},
        {"FieldAccess", AstKind::FieldAccess},
        {"BinaryExpr", AstKind::BinaryExpr},
        {"UnaryExpr", AstKind::UnaryExpr},
        {"Literal", AstKind::Literal},
        {"Identifier", AstKind::Identifier},
    };

    const auto it = kKinds.find(name);
    if (it == kKinds.end()) {
        throw std::runtime_error("Unknown AST kind label: " + name);
    }
    return it->second;
}

std::string renderAst(const AstNode &root) {
    std::ostringstream out;
    out << makeNodeLabel(root) << "\n";

    for (std::size_t i = 0; i < root.children.size(); ++i) {
        renderBranch(root.children[i], "", i + 1 == root.children.size(), out);
    }

    return out.str();
}

AstNode parseRenderedAst(const std::string &text) {
    std::istringstream input(text);
    std::string line;

    while (std::getline(input, line)) {
        line = trim(line);
        if (!line.empty()) {
            break;
        }
    }

    if (line.empty()) {
        throw std::runtime_error("Empty decorated AST input");
    }

    AstNode root = makeNodeFromLabel(line);
    std::vector<std::vector<std::size_t>> paths(1);

    while (std::getline(input, line)) {
        if (trim(line).empty()) {
            continue;
        }

        std::size_t marker = line.find("|-- ");
        if (marker == std::string::npos) {
            marker = line.find("\\-- ");
        }
        if (marker == std::string::npos || marker % 4 != 0) {
            throw std::runtime_error("Unsupported decorated AST line: " + line);
        }

        const std::size_t depth = marker / 4 + 1;
        if (depth == 0 || depth > paths.size()) {
            throw std::runtime_error("Invalid decorated AST indentation near: " +
                                     line);
        }

        AstNode *parent = nodeAtPath(root, paths[depth - 1]);
        parent->addChild(makeNodeFromLabel(line.substr(marker + 4)));

        if (paths.size() <= depth) {
            paths.resize(depth + 1);
        }
        paths[depth] = paths[depth - 1];
        paths[depth].push_back(parent->children.size() - 1);
        paths.resize(depth + 1);
    }

    return root;
}
