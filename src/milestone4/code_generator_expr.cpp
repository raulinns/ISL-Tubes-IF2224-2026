#include "code_generator.h"

#include "runtime_value.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace {

void generateExpressionAkram(const AstNode &node, const SymbolTable &symbols,
                             CodeGenContext &context);

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

TabEntry entryForSymbol(const SymbolTable &symbols, int symbolIndex,
                        const std::string &context) {
    if (symbolIndex < 0) {
        throw std::runtime_error(context + " does not have a decorated symbol index");
    }
    return symbols.tabEntry(symbolIndex);
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

int scalarAddressForVariable(const AstNode &node, const SymbolTable &symbols,
                             CodeGenContext &context) {
    if (node.kind != AstKind::Variable && node.kind != AstKind::Identifier) {
        throw std::runtime_error("Expected scalar variable/identifier as operand");
    }
    if (!node.children.empty()) {
        unsupportedCodegenNode(node, "array/record runtime layout");
    }

    const TabEntry entry =
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

void generateIdentifierExpression(const AstNode &node, const SymbolTable &symbols,
                                  CodeGenContext &context) {
    const TabEntry entry =
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
        generateCall(node, symbols, context, true);
        return;
    }

    throw std::runtime_error("Identifier is not a runtime value: " + node.text);
}

void emitOpr(CodeGenContext &context, OprCode op,
             const std::string &operatorHint = "") {
    context.emit(OpCode::OPR, 0, static_cast<int>(op), operatorHint);
}

void generateUnaryExpression(const AstNode &node, const SymbolTable &symbols,
                             CodeGenContext &context) {
    if (node.children.empty()) {
        throw std::runtime_error("UnaryExpr requires one operand");
    }

    const std::string op = lowerCopy(node.text);
    if (op == "plus") {
        generateExpressionAkram(node.children[0], symbols, context);
        return;
    }

    if (op == "minus") {
        generateExpressionAkram(node.children[0], symbols, context);
        emitOpr(context, OprCode::NEG, op);
        return;
    }

    if (op == "notsy") {
        generateExpressionAkram(node.children[0], symbols, context);
        emitLiteralFromText(context, "false", "boolean");
        emitOpr(context, OprCode::EQL, op);
        return;
    }

    throw std::runtime_error("Unsupported unary operator: " + node.text);
}

void generateBinaryExpression(const AstNode &node, const SymbolTable &symbols,
                              CodeGenContext &context) {
    if (node.children.size() < 2) {
        throw std::runtime_error("BinaryExpr requires two operands");
    }

    const std::string op = lowerCopy(node.text);
    generateExpressionAkram(node.children[0], symbols, context);
    generateExpressionAkram(node.children[1], symbols, context);

    if (op == "plus") {
        emitOpr(context, OprCode::ADD, op);
        return;
    }
    if (op == "minus") {
        emitOpr(context, OprCode::SUB, op);
        return;
    }
    if (op == "times") {
        emitOpr(context, OprCode::MUL, op);
        return;
    }
    if (op == "rdiv" || op == "idiv") {
        emitOpr(context, OprCode::DIV, op);
        return;
    }
    if (op == "imod") {
        emitOpr(context, OprCode::MOD, op);
        return;
    }
    if (op == "andsy") {
        emitOpr(context, OprCode::MUL, op);
        return;
    }
    if (op == "orsy") {
        emitOpr(context, OprCode::ADD, op);
        return;
    }
    if (op == "eql") {
        emitOpr(context, OprCode::EQL, op);
        return;
    }
    if (op == "neq") {
        emitOpr(context, OprCode::NEQ, op);
        return;
    }
    if (op == "lss") {
        emitOpr(context, OprCode::LSS, op);
        return;
    }
    if (op == "geq") {
        emitOpr(context, OprCode::GEQ, op);
        return;
    }
    if (op == "gtr") {
        emitOpr(context, OprCode::GTR, op);
        return;
    }
    if (op == "leq") {
        emitOpr(context, OprCode::LEQ, op);
        return;
    }

    throw std::runtime_error("Unsupported binary operator: " + node.text);
}

void generateExpressionAkram(const AstNode &node, const SymbolTable &symbols,
                             CodeGenContext &context) {
    switch (node.kind) {
    case AstKind::Literal:
        emitLiteralFromText(context, node.text, lowerCopy(node.inferredType));
        return;
    case AstKind::Identifier:
        generateIdentifierExpression(node, symbols, context);
        return;
    case AstKind::Variable: {
        const int address = scalarAddressForVariable(node, symbols, context);
        context.emit(OpCode::LOD, 0, address);
        return;
    }
    case AstKind::UnaryExpr:
        generateUnaryExpression(node, symbols, context);
        return;
    case AstKind::BinaryExpr:
        generateBinaryExpression(node, symbols, context);
        return;
    case AstKind::Call:
        generateCall(node, symbols, context, true);
        return;
    default:
        unsupportedCodegenNode(node, "Akram/Endra");
    }
}

} // namespace
