#include "code_generator.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string lowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

const TabEntry &entryForSymbol(const SymbolTable &symbols, int symbolIndex,
                               const char *context) {
    if (symbolIndex < 0) {
        throw std::runtime_error(std::string(context) +
                                 " does not have a decorated symbol index");
    }
    return symbols.tabEntry(symbolIndex);
}

int symbolIndexForCall(const AstNode &node, const SymbolTable &symbols) {
    if (node.symbolIndex >= 0) {
        return node.symbolIndex;
    }

    const int index = symbols.lookup(node.text);
    if (index < 0) {
        throw std::runtime_error("Callable symbol not found: " + node.text);
    }
    return index;
}

std::vector<int> parameterSymbolsFor(const TabEntry &callable,
                                     const SymbolTable &symbols) {
    std::vector<int> params;
    if (callable.ref <= 0) {
        return params;
    }

    int paramIndex = symbols.btabEntry(callable.ref).lpar;
    while (paramIndex > 0) {
        const TabEntry &param = symbols.tabEntry(paramIndex);
        if (param.obj != ObjectKind::Parameter) {
            break;
        }
        params.push_back(paramIndex);
        paramIndex = param.link;
    }
    std::reverse(params.begin(), params.end());
    return params;
}

int scalarAddressForReadTarget(const AstNode &node, const SymbolTable &symbols,
                               CodeGenContext &context,
                               std::string &declaredType) {
    if (node.kind != AstKind::Variable && node.kind != AstKind::Identifier) {
        throw std::runtime_error("readln argument must be a scalar variable");
    }
    if (!node.children.empty()) {
        throw std::runtime_error("readln does not support array/record targets yet");
    }

    const TabEntry &entry =
        entryForSymbol(symbols, node.symbolIndex, "readln target");
    if (entry.obj != ObjectKind::Variable && entry.obj != ObjectKind::Parameter) {
        throw std::runtime_error("readln argument is not assignable: " + node.text);
    }

    if (!context.hasRuntimeAddress(node.symbolIndex)) {
        context.allocateRuntimeAddress(node.symbolIndex);
    }
    declaredType = typeKindToString(entry.type);
    return context.runtimeAddressOf(node.symbolIndex);
}

void emitWriteCall(const AstNode &node, const SymbolTable &symbols,
                   CodeGenContext &context, bool withFinalNewline) {
    if (node.children.empty()) {
        if (withFinalNewline) {
            context.emit(OpCode::OPR, 0, static_cast<int>(OprCode::WRTLN),
                         "writeln empty");
            return;
        }
        return;
    }

    for (std::size_t i = 0; i < node.children.size(); ++i) {
        generateExpression(node.children[i], symbols, context);
        const bool last = i + 1 == node.children.size();
        const OprCode op =
            withFinalNewline && last ? OprCode::WRTLN : OprCode::WRT;
        context.emit(OpCode::OPR, 0, static_cast<int>(op),
                     op == OprCode::WRTLN ? "writeln" : "write");
    }
}

void emitReadlnCall(const AstNode &node, const SymbolTable &symbols,
                    CodeGenContext &context) {
    if (node.children.empty()) {
        throw std::runtime_error("readln requires at least one variable argument");
    }

    for (const AstNode &arg : node.children) {
        std::string declaredType;
        const int address =
            scalarAddressForReadTarget(arg, symbols, context, declaredType);
        context.emit(Instruction(OpCode::OPR, address,
                                 static_cast<int>(OprCode::READLN),
                                 declaredType, "readln"));
    }
}

} // namespace

void generateCall(const AstNode &node, const SymbolTable &symbols,
                  CodeGenContext &context, bool asExpression) {
    if (node.kind != AstKind::Call && node.kind != AstKind::Identifier) {
        throw std::runtime_error("Expected Call or Identifier node for call codegen");
    }

    const std::string normalized = lowerCopy(node.text);
    if (normalized == "writeln") {
        if (asExpression) {
            throw std::runtime_error("writeln cannot be used as an expression");
        }
        emitWriteCall(node, symbols, context, true);
        return;
    }
    if (normalized == "write") {
        if (asExpression) {
            throw std::runtime_error("write cannot be used as an expression");
        }
        emitWriteCall(node, symbols, context, false);
        return;
    }
    if (normalized == "readln") {
        if (asExpression) {
            throw std::runtime_error("readln cannot be used as an expression");
        }
        emitReadlnCall(node, symbols, context);
        return;
    }

    const int callSymbol = symbolIndexForCall(node, symbols);
    const TabEntry &callable =
        entryForSymbol(symbols, callSymbol, "Callable expression");
    if (callable.obj != ObjectKind::Procedure && callable.obj != ObjectKind::Function) {
        throw std::runtime_error("Identifier is not callable: " + node.text);
    }
    if (asExpression && callable.obj != ObjectKind::Function) {
        throw std::runtime_error("Procedure has no return value: " + node.text);
    }

    const std::vector<int> params = parameterSymbolsFor(callable, symbols);
    if (params.size() != node.children.size()) {
        throw std::runtime_error("Call argument count mismatch for " + node.text);
    }
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        generateExpression(node.children[i], symbols, context);
        if (!context.hasRuntimeAddress(params[i])) {
            context.allocateRuntimeAddress(params[i]);
        }
        context.emit(OpCode::STO, 0, context.runtimeAddressOf(params[i]),
                     "argument " + std::to_string(i + 1));
    }

    if (!context.hasSubprogramEntry(callSymbol)) {
        throw std::runtime_error("Subprogram entry point is not available: " +
                                 node.text);
    }
    context.emit(OpCode::CAL, 0, context.subprogramEntryOf(callSymbol),
                 "call " + callable.identifier);

    if (callable.obj == ObjectKind::Function) {
        if (!context.hasRuntimeAddress(callSymbol)) {
            context.allocateRuntimeAddress(callSymbol);
        }
        if (asExpression) {
            context.emit(OpCode::LOD, 0, context.runtimeAddressOf(callSymbol),
                         "function result " + callable.identifier);
        }
    }
}
