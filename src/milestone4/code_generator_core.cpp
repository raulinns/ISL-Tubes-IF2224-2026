#include "code_generator.h"

#include "runtime_value.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct RuntimeType {
    TypeKind kind = TypeKind::None;
    int ref = 0;
};

std::vector<int> currentFunctionSymbols;

std::string lowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

const TabEntry &symbol(const SymbolTable &symbols, int index,
                       const std::string &context) {
    if (index < 0) {
        throw std::runtime_error(context + " is missing a symbol reference");
    }
    return symbols.tabEntry(index);
}

RuntimeType typeOfEntry(const TabEntry &entry) {
    return {entry.type, entry.typeRef};
}

int typeSize(const RuntimeType &type, const SymbolTable &symbols) {
    return runtimeTypeSize(type.kind, type.ref, symbols);
}

bool isComposite(const RuntimeType &type) {
    return type.kind == TypeKind::Array || type.kind == TypeKind::Record;
}

int lexicalDifference(const CodeGenContext &context, const TabEntry &entry) {
    const int difference = context.currentLexicalLevel() - entry.lev;
    if (difference < 0) {
        throw std::runtime_error("Invalid lexical reference to " +
                                 entry.identifier);
    }
    return difference;
}

bool isCurrentFunctionResult(int symbolIndex) {
    return !currentFunctionSymbols.empty() &&
           currentFunctionSymbols.back() == symbolIndex;
}

int literalPayload(const std::string &text, const std::string &type) {
    try {
        const RuntimeValue value = RuntimeValue::parseLiteral(text, type);
        if (value.kind() == RuntimeValueKind::Integer) return value.asInteger();
        if (value.kind() == RuntimeValueKind::Boolean)
            return value.asBoolean() ? 1 : 0;
        if (value.kind() == RuntimeValueKind::Char)
            return static_cast<unsigned char>(value.asChar());
    } catch (const std::exception &) {
    }
    return 0;
}

void emitOpr(CodeGenContext &context, OprCode code,
             std::vector<int> extra = {}) {
    context.emit(OpCode::OPR, 0, static_cast<int>(code), std::move(extra));
}

std::vector<int> parameterSymbols(const TabEntry &callable,
                                  const SymbolTable &symbols) {
    std::vector<int> parameters;
    if (callable.ref <= 0) {
        return parameters;
    }
    int index = symbols.btabEntry(callable.ref).lpar;
    while (index > 0 && symbols.tabEntry(index).obj == ObjectKind::Parameter) {
        parameters.push_back(index);
        index = symbols.tabEntry(index).link;
    }
    std::reverse(parameters.begin(), parameters.end());
    return parameters;
}

RuntimeType emitAddress(const AstNode &node, const SymbolTable &symbols,
                        CodeGenContext &context) {
    if (node.kind != AstKind::Variable && node.kind != AstKind::Identifier) {
        throw std::runtime_error("Expected an assignable variable");
    }
    const TabEntry &base = symbol(symbols, node.symbolIndex, "Variable");
    if (base.obj != ObjectKind::Variable &&
        base.obj != ObjectKind::Parameter &&
        !isCurrentFunctionResult(node.symbolIndex)) {
        throw std::runtime_error("Identifier is not assignable: " + node.text);
    }
    const int level = isCurrentFunctionResult(node.symbolIndex)
                          ? 0
                          : lexicalDifference(context, base);
    context.emit(OpCode::LDA, level, base.adr);
    RuntimeType current = typeOfEntry(base);
    for (const AstNode &component : node.children) {
        if (component.kind == AstKind::IndexAccess) {
            for (const AstNode &index : component.children) {
                if (current.kind != TypeKind::Array) {
                    throw std::runtime_error("Index access on non-array value");
                }
                const ATabEntry &array = symbols.atabEntry(current.ref);
                generateExpression(index, symbols, context);
                context.emit(OpCode::IXA, 0, array.low,
                             std::vector<int>{array.high, array.elsz});
                current = {array.etyp, array.eref};
            }
        } else if (component.kind == AstKind::FieldAccess) {
            if (current.kind != TypeKind::Record || component.symbolIndex < 0) {
                throw std::runtime_error("Invalid record field access");
            }
            const TabEntry &field = symbols.tabEntry(component.symbolIndex);
            context.emit(OpCode::ADI, 0, field.adr);
            current = typeOfEntry(field);
        }
    }
    return current;
}

void emitVariableValue(const AstNode &node, const SymbolTable &symbols,
                       CodeGenContext &context) {
    const TabEntry &entry = symbol(symbols, node.symbolIndex, "Variable");
    RuntimeType type = typeOfEntry(entry);
    if (node.children.empty() && !isComposite(type) &&
        !isCurrentFunctionResult(node.symbolIndex)) {
        context.emit(OpCode::LOD, lexicalDifference(context, entry), entry.adr);
        return;
    }
    type = emitAddress(node, symbols, context);
    if (isComposite(type)) {
        context.emit(OpCode::BLD, 0, typeSize(type, symbols));
    } else {
        context.emit(OpCode::LDI, 0, 0);
    }
}

void generateAssignment(const AstNode &node, const SymbolTable &symbols,
                        CodeGenContext &context) {
    if (node.children.size() != 2) {
        throw std::runtime_error("Assignment requires target and value");
    }
    generateExpression(node.children[1], symbols, context);
    if (node.children[0].children.empty() &&
        !isCurrentFunctionResult(node.children[0].symbolIndex)) {
        const TabEntry &target =
            symbol(symbols, node.children[0].symbolIndex, "Assignment target");
        const RuntimeType type = typeOfEntry(target);
        if (!isComposite(type)) {
            context.emit(OpCode::STO, lexicalDifference(context, target),
                         target.adr);
            return;
        }
    }
    const RuntimeType target =
        emitAddress(node.children[0], symbols, context);
    if (isComposite(target)) {
        context.emit(OpCode::BST, 0, typeSize(target, symbols));
    } else {
        context.emit(OpCode::STI, 0, 0);
    }
}

void generateIf(const AstNode &node, const SymbolTable &symbols,
                CodeGenContext &context) {
    generateExpression(node.children.at(0), symbols, context);
    const int falseJump = context.emit(OpCode::JPC, 0, 0);
    generateStatement(node.children.at(1), symbols, context);
    if (node.children.size() > 2) {
        const int endJump = context.emit(OpCode::JMP, 0, 0);
        context.patch(falseJump, context.nextInstructionIndex());
        generateStatement(node.children[2], symbols, context);
        context.patch(endJump, context.nextInstructionIndex());
    } else {
        context.patch(falseJump, context.nextInstructionIndex());
    }
}

void generateWhile(const AstNode &node, const SymbolTable &symbols,
                   CodeGenContext &context) {
    const int start = context.nextInstructionIndex();
    generateExpression(node.children.at(0), symbols, context);
    const int finish = context.emit(OpCode::JPC, 0, 0);
    generateStatement(node.children.at(1), symbols, context);
    context.emit(OpCode::JMP, 0, start);
    context.patch(finish, context.nextInstructionIndex());
}

void generateRepeat(const AstNode &node, const SymbolTable &symbols,
                    CodeGenContext &context) {
    const int start = context.nextInstructionIndex();
    generateStatement(node.children.at(0), symbols, context);
    generateExpression(node.children.at(1), symbols, context);
    context.emit(OpCode::JPC, 0, start);
}

void generateFor(const AstNode &node, const SymbolTable &symbols,
                 CodeGenContext &context) {
    const bool ascending = lowerCopy(node.text) == "to";
    generateExpression(node.children.at(1), symbols, context);
    emitAddress(node.children.at(0), symbols, context);
    context.emit(OpCode::STI, 0, 0);
    const int start = context.nextInstructionIndex();
    emitVariableValue(node.children.at(0), symbols, context);
    generateExpression(node.children.at(2), symbols, context);
    emitOpr(context, ascending ? OprCode::LEQ : OprCode::GEQ);
    const int finish = context.emit(OpCode::JPC, 0, 0);
    generateStatement(node.children.at(3), symbols, context);
    emitVariableValue(node.children.at(0), symbols, context);
    context.emitLiteral(0, 1, "1");
    emitOpr(context, ascending ? OprCode::ADD : OprCode::SUB);
    emitAddress(node.children.at(0), symbols, context);
    context.emit(OpCode::STI, 0, 0);
    context.emit(OpCode::JMP, 0, start);
    context.patch(finish, context.nextInstructionIndex());
}

void generateCase(const AstNode &node, const SymbolTable &symbols,
                  CodeGenContext &context) {
    std::vector<int> endJumps;
    for (std::size_t i = 1; i < node.children.size(); ++i) {
        const AstNode &branch = node.children[i];
        if (branch.children.size() < 2) {
            continue;
        }
        for (const AstNode &label : branch.children[0].children) {
            generateExpression(node.children[0], symbols, context);
            generateExpression(label, symbols, context);
            emitOpr(context, OprCode::EQL);
            const int next = context.emit(OpCode::JPC, 0, 0);
            generateStatement(branch.children[1], symbols, context);
            endJumps.push_back(context.emit(OpCode::JMP, 0, 0));
            context.patch(next, context.nextInstructionIndex());
        }
    }
    for (const int jump : endJumps) {
        context.patch(jump, context.nextInstructionIndex());
    }
}

int inputTypeCode(TypeKind type) {
    switch (type) {
    case TypeKind::Integer:
    case TypeKind::Subrange:
    case TypeKind::Enum:
        return 1;
    case TypeKind::Real:
        return 2;
    case TypeKind::Boolean:
        return 3;
    case TypeKind::Char:
        return 4;
    case TypeKind::String:
        return 5;
    default:
        throw std::runtime_error("readln only accepts scalar variables");
    }
}

void generateBuiltinCall(const AstNode &node, const SymbolTable &symbols,
                         CodeGenContext &context,
                         const std::string &name) {
    if (name == "readln") {
        if (node.children.empty()) {
            throw std::runtime_error("readln requires at least one argument");
        }
        for (const AstNode &argument : node.children) {
            const RuntimeType type = emitAddress(argument, symbols, context);
            emitOpr(context, OprCode::READLN, {inputTypeCode(type.kind)});
        }
        return;
    }
    const bool newline = name == "writeln";
    if (node.children.empty()) {
        if (newline) {
            emitOpr(context, OprCode::WRTLN, {1});
        }
        return;
    }
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        generateExpression(node.children[i], symbols, context);
        emitOpr(context, newline && i + 1 == node.children.size()
                             ? OprCode::WRTLN
                             : OprCode::WRT);
    }
}

void generateDeclarations(const AstNode &declarations,
                          const SymbolTable &symbols,
                          CodeGenContext &context);

void generateSubprogram(const AstNode &decl, const SymbolTable &symbols,
                        CodeGenContext &context) {
    const TabEntry &callable =
        symbol(symbols, decl.symbolIndex, "Subprogram declaration");
    context.bindSubprogramEntry(decl.symbolIndex,
                                context.nextInstructionIndex());

    const int previousLevel = context.currentLexicalLevel();
    context.setCurrentLexicalLevel(callable.lev + 1);
    if (decl.kind == AstKind::FunctionDecl) {
        currentFunctionSymbols.push_back(decl.symbolIndex);
    }

    const std::size_t declarationsIndex =
        decl.kind == AstKind::FunctionDecl ? 2U : 1U;
    int bodyJump = -1;
    if (decl.children.size() > declarationsIndex) {
        const AstNode &nested = decl.children[declarationsIndex];
        const bool hasNested = std::any_of(
            nested.children.begin(), nested.children.end(),
            [](const AstNode &node) {
                return node.kind == AstKind::ProcedureDecl ||
                       node.kind == AstKind::FunctionDecl;
            });
        if (hasNested) {
            bodyJump = context.emit(OpCode::JMP, 0, 0);
            generateDeclarations(nested, symbols, context);
            context.patch(bodyJump, context.nextInstructionIndex());
        }
    }

    const BTabEntry &block = symbols.btabEntry(callable.ref);
    context.emit(OpCode::INT, 0,
                 CodeGenContext::kFrameHeaderSize + block.psze + block.vsze);

    const std::vector<int> params = parameterSymbols(callable, symbols);
    for (auto it = params.rbegin(); it != params.rend(); ++it) {
        const TabEntry &parameter = symbols.tabEntry(*it);
        const RuntimeType type = typeOfEntry(parameter);
        context.emit(OpCode::LDA, 0, parameter.adr);
        if (isComposite(type)) {
            context.emit(OpCode::BST, 0, typeSize(type, symbols));
        } else {
            context.emit(OpCode::STI, 0, 0);
        }
    }

    const std::size_t bodyIndex =
        decl.kind == AstKind::FunctionDecl ? 3U : 2U;
    if (decl.children.size() > bodyIndex) {
        generateStatement(decl.children[bodyIndex], symbols, context);
    }
    const int resultSize =
        decl.kind == AstKind::FunctionDecl
            ? runtimeTypeSize(callable.type, callable.typeRef, symbols)
            : 0;
    context.emit(OpCode::RET, 0,
                 resultSize > 0 ? callable.adr : 0,
                 std::vector<int>{resultSize});

    if (decl.kind == AstKind::FunctionDecl) {
        currentFunctionSymbols.pop_back();
    }
    context.setCurrentLexicalLevel(previousLevel);
}

void generateDeclarations(const AstNode &declarations,
                          const SymbolTable &symbols,
                          CodeGenContext &context) {
    for (const AstNode &decl : declarations.children) {
        if (decl.kind == AstKind::ProcedureDecl ||
            decl.kind == AstKind::FunctionDecl) {
            generateSubprogram(decl, symbols, context);
        }
    }
}

} // namespace

CodeGeneratorResult generateIntermediateCode(const AstNode &decoratedAst,
                                             const SymbolTable &symbols) {
    CodeGenContext context;
    generateProgram(decoratedAst, symbols, context);
    context.validateCallPatches();
    return {context, context.code()};
}

void generateProgram(const AstNode &decoratedAst, const SymbolTable &symbols,
                     CodeGenContext &context) {
    if (decoratedAst.kind != AstKind::Program) {
        throw std::runtime_error("Code generation requires Program root");
    }
    currentFunctionSymbols.clear();
    context.reset();
    bool hasSubprogram = false;
    if (!decoratedAst.children.empty()) {
        hasSubprogram = std::any_of(
            decoratedAst.children[0].children.begin(),
            decoratedAst.children[0].children.end(), [](const AstNode &node) {
                return node.kind == AstKind::ProcedureDecl ||
                       node.kind == AstKind::FunctionDecl;
            });
    }
    int mainJump = -1;
    if (hasSubprogram) {
        mainJump = context.emit(OpCode::JMP, 0, 0);
        generateDeclarations(decoratedAst.children[0], symbols, context);
        context.patch(mainJump, context.nextInstructionIndex());
    }
    context.setCurrentLexicalLevel(0);
    const int globalSize =
        CodeGenContext::kFrameHeaderSize +
        (symbols.btab().empty() ? 0 : symbols.btabEntry(0).vsze);
    context.emit(OpCode::INT, 0, globalSize);
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
        generateIf(node, symbols, context);
        return;
    case AstKind::WhileStmt:
        generateWhile(node, symbols, context);
        return;
    case AstKind::RepeatStmt:
        generateRepeat(node, symbols, context);
        return;
    case AstKind::ForStmt:
        generateFor(node, symbols, context);
        return;
    case AstKind::CaseStmt:
        generateCase(node, symbols, context);
        return;
    case AstKind::Call:
    case AstKind::Identifier:
        generateCall(node, symbols, context, false);
        return;
    default:
        throw std::runtime_error("Unsupported statement node: " +
                                 astKindToString(node.kind));
    }
}

void generateExpression(const AstNode &node, const SymbolTable &symbols,
                        CodeGenContext &context) {
    switch (node.kind) {
    case AstKind::Literal:
        context.emitLiteral(0, literalPayload(node.text, node.inferredType),
                            node.text);
        return;
    case AstKind::Variable:
        emitVariableValue(node, symbols, context);
        return;
    case AstKind::Identifier: {
        const TabEntry &entry =
            symbol(symbols, node.symbolIndex, "Identifier expression");
        if (entry.obj == ObjectKind::Constant) {
            context.emitLiteral(
                0, literalPayload(entry.literalValue,
                                  typeKindToString(entry.type)),
                entry.literalValue);
        } else if (entry.obj == ObjectKind::Variable ||
                   entry.obj == ObjectKind::Parameter) {
            emitVariableValue(node, symbols, context);
        } else if (entry.obj == ObjectKind::Function) {
            generateCall(node, symbols, context, true);
        } else {
            throw std::runtime_error("Identifier is not a runtime value: " +
                                     node.text);
        }
        return;
    }
    case AstKind::Call:
        generateCall(node, symbols, context, true);
        return;
    case AstKind::UnaryExpr: {
        generateExpression(node.children.at(0), symbols, context);
        const std::string op = lowerCopy(node.text);
        if (op == "minus") emitOpr(context, OprCode::NEG);
        else if (op == "notsy") emitOpr(context, OprCode::NOT);
        else if (op != "plus")
            throw std::runtime_error("Unsupported unary operator: " + node.text);
        return;
    }
    case AstKind::BinaryExpr: {
        generateExpression(node.children.at(0), symbols, context);
        generateExpression(node.children.at(1), symbols, context);
        const std::string op = lowerCopy(node.text);
        if (op == "plus") emitOpr(context, OprCode::ADD);
        else if (op == "minus") emitOpr(context, OprCode::SUB);
        else if (op == "times") emitOpr(context, OprCode::MUL);
        else if (op == "rdiv") emitOpr(context, OprCode::DIV);
        else if (op == "idiv") emitOpr(context, OprCode::IDIV);
        else if (op == "imod") emitOpr(context, OprCode::MOD);
        else if (op == "andsy") emitOpr(context, OprCode::AND);
        else if (op == "orsy") emitOpr(context, OprCode::OR);
        else if (op == "eql") emitOpr(context, OprCode::EQL);
        else if (op == "neq") emitOpr(context, OprCode::NEQ);
        else if (op == "lss") emitOpr(context, OprCode::LSS);
        else if (op == "geq") emitOpr(context, OprCode::GEQ);
        else if (op == "gtr") emitOpr(context, OprCode::GTR);
        else if (op == "leq") emitOpr(context, OprCode::LEQ);
        else throw std::runtime_error("Unsupported binary operator: " + node.text);
        return;
    }
    default:
        throw std::runtime_error("Unsupported expression node: " +
                                 astKindToString(node.kind));
    }
}

void generateCall(const AstNode &node, const SymbolTable &symbols,
                  CodeGenContext &context, bool asExpression) {
    const std::string name = lowerCopy(node.text);
    if (name == "readln" || name == "write" || name == "writeln") {
        if (asExpression) {
            throw std::runtime_error(name + " cannot be used as an expression");
        }
        generateBuiltinCall(node, symbols, context, name);
        return;
    }
    const TabEntry &callable =
        symbol(symbols, node.symbolIndex, "Procedure/function call");
    if (callable.obj != ObjectKind::Procedure &&
        callable.obj != ObjectKind::Function) {
        throw std::runtime_error("Identifier is not callable: " + node.text);
    }
    if (asExpression && callable.obj != ObjectKind::Function) {
        throw std::runtime_error("Procedure has no return value: " + node.text);
    }
    const std::vector<int> params = parameterSymbols(callable, symbols);
    if (params.size() != node.children.size()) {
        throw std::runtime_error("Call argument count mismatch for " + node.text);
    }
    int argumentSlots = 0;
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        generateExpression(node.children[i], symbols, context);
        const TabEntry &parameter = symbols.tabEntry(params[i]);
        argumentSlots +=
            runtimeTypeSize(parameter.type, parameter.typeRef, symbols);
    }
    const int level = lexicalDifference(context, callable);
    const int instruction =
        context.emit(OpCode::CAL, level, 0,
                     std::vector<int>{argumentSlots});
    context.addCallPatch(node.symbolIndex, instruction);
}
