#include "code_generator.h"

#include "runtime_value.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace {

std::string lowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

[[noreturn]] void unsupportedCodegenNode(const AstNode &node,
                                         const std::string &ownerHint) {
    throw std::runtime_error("Milestone 4 placeholder: code generation for " +
                             astKindToString(node.kind) +
                             (node.text.empty() ? "" : "(" + node.text + ")") +
                             " belongs to " + ownerHint + " integration");
}

const TabEntry &entryForSymbol(const SymbolTable &symbols, int symbolIndex,
                               const std::string &context) {
    if (symbolIndex < 0) {
        throw std::runtime_error(context + " does not have a decorated symbol index");
    }
    return symbols.tabEntry(symbolIndex);
}

bool isSimpleRuntimeType(TypeKind type) {
    return type == TypeKind::Integer || type == TypeKind::Real ||
           type == TypeKind::Boolean || type == TypeKind::Char ||
           type == TypeKind::String || type == TypeKind::Subrange ||
           type == TypeKind::Enum;
}

int intPayloadForLiteral(const std::string &literal,
                         const std::string &declaredType) {
    try {
        const RuntimeValue value = RuntimeValue::parseLiteral(literal, declaredType);
        switch (value.kind()) {
        case RuntimeValueKind::Integer:
            return value.asInteger();
        case RuntimeValueKind::Boolean:
            return value.asBoolean() ? 1 : 0;
        case RuntimeValueKind::Char:
            return static_cast<unsigned char>(value.asChar());
        case RuntimeValueKind::Real:
        case RuntimeValueKind::String:
        case RuntimeValueKind::Uninitialized:
            return 0;
        }
    } catch (const std::exception &) {
        return 0;
    }

    return 0;
}

void emitLiteralFromText(CodeGenContext &context, const std::string &literal,
                         const std::string &declaredType,
                         const std::string &comment = "") {
    context.emitLiteral(0, intPayloadForLiteral(literal, declaredType), literal,
                        comment);
}

void allocateDeclarations(const AstNode &node, const SymbolTable &symbols,
                          CodeGenContext &context) {
    if (node.kind != AstKind::Declarations) {
        return;
    }

    for (const AstNode &decl : node.children) {
        switch (decl.kind) {
        case AstKind::ConstDecl:
        case AstKind::TypeDecl:
            break;
        case AstKind::VarDecl: {
            const TabEntry &entry =
                entryForSymbol(symbols, decl.symbolIndex, "Variable declaration");
            if (!isSimpleRuntimeType(entry.type)) {
                unsupportedCodegenNode(decl, "array/record runtime layout");
            }
            context.allocateRuntimeAddress(decl.symbolIndex);
            break;
        }
        case AstKind::ProcedureDecl:
        case AstKind::FunctionDecl:
            unsupportedCodegenNode(decl, "Endra");
        default:
            break;
        }
    }
}

int scalarAddressForVariable(const AstNode &node, const SymbolTable &symbols,
                             CodeGenContext &context) {
    if (node.kind != AstKind::Variable && node.kind != AstKind::Identifier) {
        throw std::runtime_error("Expected scalar variable/identifier as l-value");
    }
    if (!node.children.empty()) {
        unsupportedCodegenNode(node, "array/record runtime layout");
    }

    const TabEntry &entry =
        entryForSymbol(symbols, node.symbolIndex, "Variable access");
    if (entry.obj != ObjectKind::Variable && entry.obj != ObjectKind::Parameter) {
        throw std::runtime_error("Identifier is not stored in runtime memory: " +
                                 node.text);
    }
    if (!context.hasRuntimeAddress(node.symbolIndex)) {
        context.allocateRuntimeAddress(node.symbolIndex);
    }
    return context.runtimeAddressOf(node.symbolIndex);
}

void generateDeclarations(const AstNode &node) {
    if (node.kind != AstKind::Declarations) {
        return;
    }

    for (const AstNode &decl : node.children) {
        switch (decl.kind) {
        case AstKind::ConstDecl:
        case AstKind::TypeDecl:
        case AstKind::VarDecl:
            break;
        case AstKind::ProcedureDecl:
        case AstKind::FunctionDecl:
            unsupportedCodegenNode(decl, "Endra");
        default:
            break;
        }
    }
}

void generateIdentifierExpression(const AstNode &node, const SymbolTable &symbols,
                                  CodeGenContext &context) {
    const TabEntry &entry =
        entryForSymbol(symbols, node.symbolIndex, "Identifier expression");

    if (entry.obj == ObjectKind::Constant) {
        emitLiteralFromText(context, entry.literalValue, typeKindToString(entry.type));
        return;
    }

    if (entry.obj == ObjectKind::Variable || entry.obj == ObjectKind::Parameter) {
        const int address = scalarAddressForVariable(node, symbols, context);
        context.emit(OpCode::LOD, 0, address);
        return;
    }

    if (entry.obj == ObjectKind::Function) {
        unsupportedCodegenNode(node, "Endra");
    }

    throw std::runtime_error("Identifier is not a runtime value: " + node.text);
}

void generateAssignment(const AstNode &node, const SymbolTable &symbols,
                        CodeGenContext &context) {
    if (node.children.size() < 2) {
        throw std::runtime_error("AssignStmt requires target and expression");
    }

    generateExpression(node.children[1], symbols, context);
    const int address = scalarAddressForVariable(node.children[0], symbols, context);
    context.emit(OpCode::STO, 0, address);
}

} // namespace

CodeGeneratorResult generateIntermediateCode(const AstNode &decoratedAst,
                                             const SymbolTable &symbols) {
    CodeGenContext context;
    generateProgram(decoratedAst, symbols, context);
    return {context, context.code()};
}

void generateProgram(const AstNode &decoratedAst, const SymbolTable &symbols,
                     CodeGenContext &context) {
    if (decoratedAst.kind != AstKind::Program) {
        throw std::runtime_error("Intermediate code generation expects Program root");
    }

    context.reset();

    if (!decoratedAst.children.empty()) {
        allocateDeclarations(decoratedAst.children[0], symbols, context);
    }

    context.emit(OpCode::INT, 0, context.frameSize());

    if (!decoratedAst.children.empty()) {
        generateDeclarations(decoratedAst.children[0]);
    }
    if (decoratedAst.children.size() > 1) {
        generateStatement(decoratedAst.children[1], symbols, context);
    }

    context.emit(OpCode::RET, 0, 0);
}

void generateStatement(const AstNode &node, const SymbolTable &symbols,
                       CodeGenContext &context) {
    switch (node.kind) {
    case AstKind::CompoundStmt:
    case AstKind::StatementList:
        for (const AstNode &child : node.children) {
            generateStatement(child, symbols, context);
        }
        return;
    case AstKind::EmptyStmt:
        return;
    case AstKind::AssignStmt:
        generateAssignment(node, symbols, context);
        return;
    case AstKind::IfStmt:
    case AstKind::WhileStmt:
    case AstKind::RepeatStmt:
    case AstKind::ForStmt:
    case AstKind::CaseStmt:
        unsupportedCodegenNode(node, "Steven");
    case AstKind::Call:
        generateCall(node, symbols, context, false);
        return;
    default:
        unsupportedCodegenNode(node, "Akram/Endra/Steven");
    }
}

void generateCall(const AstNode &node, const SymbolTable &, CodeGenContext &,
                  bool) {
    unsupportedCodegenNode(node, "Endra");
}
