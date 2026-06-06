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

bool hasSubprogramDeclarations(const AstNode &node) {
    if (node.kind != AstKind::Declarations) {
        return false;
    }

    for (const AstNode &decl : node.children) {
        if (decl.kind == AstKind::ProcedureDecl ||
            decl.kind == AstKind::FunctionDecl) {
            return true;
        }
    }
    return false;
}

std::vector<int> parameterSymbolsFromDecl(const AstNode &decl) {
    std::vector<int> params;
    if (decl.children.empty() || decl.children[0].kind != AstKind::Parameters) {
        return params;
    }

    for (const AstNode &param : decl.children[0].children) {
        if (param.kind == AstKind::ParameterDecl && param.symbolIndex >= 0) {
            params.push_back(param.symbolIndex);
        }
    }
    return params;
}

void allocateParameterStorage(const AstNode &node, CodeGenContext &context) {
    if (node.kind != AstKind::Parameters) {
        return;
    }

    for (const AstNode &param : node.children) {
        if (param.kind == AstKind::ParameterDecl) {
            context.allocateRuntimeAddress(param.symbolIndex);
        }
    }
}

void allocateSubprogramStorage(const AstNode &decl, const SymbolTable &symbols,
                               CodeGenContext &context);
void generateDeclarations(const AstNode &node, const SymbolTable &symbols,
                          CodeGenContext &context);

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
            const TabEntry entry =
                entryForSymbol(symbols, decl.symbolIndex, "Variable declaration");
            if (!isSimpleRuntimeType(entry.type)) {
                unsupportedCodegenNode(decl, "array/record runtime layout");
            }
            context.allocateRuntimeAddress(decl.symbolIndex);
            break;
        }
        case AstKind::ProcedureDecl:
        case AstKind::FunctionDecl:
            allocateSubprogramStorage(decl, symbols, context);
            break;
        default:
            break;
        }
    }
}

void allocateSubprogramStorage(const AstNode &decl, const SymbolTable &symbols,
                               CodeGenContext &context) {
    context.beginFrameLayout();

    if (decl.kind == AstKind::FunctionDecl) {
        context.allocateRuntimeAddress(decl.symbolIndex);
    }

    if (!decl.children.empty()) {
        allocateParameterStorage(decl.children[0], context);
    }

    const std::size_t declarationsIndex =
        decl.kind == AstKind::FunctionDecl ? 2U : 1U;
    if (decl.children.size() > declarationsIndex) {
        allocateDeclarations(decl.children[declarationsIndex], symbols, context);
    }

    context.bindFrameSizeForSubprogram(decl.symbolIndex, context.endFrameLayout());
}

int scalarAddressForVariable(const AstNode &node, const SymbolTable &symbols,
                             CodeGenContext &context) {
    if (node.kind != AstKind::Variable && node.kind != AstKind::Identifier) {
        throw std::runtime_error("Expected scalar variable/identifier as l-value");
    }
    if (!node.children.empty()) {
        unsupportedCodegenNode(node, "array/record runtime layout");
    }

    const TabEntry entry =
        entryForSymbol(symbols, node.symbolIndex, "Variable access");
    if (entry.obj != ObjectKind::Variable && entry.obj != ObjectKind::Parameter &&
        entry.obj != ObjectKind::Function) {
        throw std::runtime_error("Identifier is not stored in runtime memory: " +
                                 node.text);
    }
    if (!context.hasRuntimeAddress(node.symbolIndex)) {
        context.allocateRuntimeAddress(node.symbolIndex);
    }
    return node.symbolIndex;
}

void generateSubprogramDeclaration(const AstNode &decl, const SymbolTable &symbols,
                                   CodeGenContext &context) {
    if (decl.symbolIndex < 0) {
        throw std::runtime_error("Subprogram declaration is missing symbol index");
    }

    context.bindSubprogramEntry(decl.symbolIndex, context.nextInstructionIndex());

    const std::size_t declarationsIndex =
        decl.kind == AstKind::FunctionDecl ? 2U : 1U;
    if (decl.children.size() > declarationsIndex &&
        hasSubprogramDeclarations(decl.children[declarationsIndex])) {
        const int bodyJump =
            context.emit(OpCode::JMP, 0, 0, "body entry " + decl.text);
        generateDeclarations(decl.children[declarationsIndex], symbols, context);
        context.patch(bodyJump, context.nextInstructionIndex());
    }

    context.emit(OpCode::INT, 0, context.frameSizeForSubprogram(decl.symbolIndex),
                 "enter " + decl.text);
    const std::vector<int> params = parameterSymbolsFromDecl(decl);
    for (auto it = params.rbegin(); it != params.rend(); ++it) {
        context.emit(OpCode::STO, context.runtimeLevelOf(*it),
                     context.runtimeAddressOf(*it),
                     "param " + std::to_string(*it));
    }

    const std::size_t bodyIndex = decl.kind == AstKind::FunctionDecl ? 3U : 2U;
    if (decl.children.size() > bodyIndex) {
        generateStatement(decl.children[bodyIndex], symbols, context);
    }

    const int returnOffset = decl.kind == AstKind::FunctionDecl
                                 ? context.runtimeAddressOf(decl.symbolIndex)
                                 : -1;
    context.emit(OpCode::RET, 0, returnOffset, "return " + decl.text);
}

void generateDeclarations(const AstNode &node, const SymbolTable &symbols,
                          CodeGenContext &context) {
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
            generateSubprogramDeclaration(decl, symbols, context);
            break;
        default:
            break;
        }
    }
}

void generateAssignment(const AstNode &node, const SymbolTable &symbols,
                        CodeGenContext &context) {
    if (node.children.size() < 2) {
        throw std::runtime_error("AssignStmt requires target and expression");
    }

    generateExpression(node.children[1], symbols, context);
    const int symbolIndex =
        scalarAddressForVariable(node.children[0], symbols, context);
    context.emit(OpCode::STO, context.runtimeLevelOf(symbolIndex),
                 context.runtimeAddressOf(symbolIndex));
}

void emitOpr(CodeGenContext &context, OprCode op,
             const std::string &operatorHint = "") {
    context.emit(OpCode::OPR, 0, static_cast<int>(op), operatorHint);
}

void generateIfStatement(const AstNode &node, const SymbolTable &symbols,
                         CodeGenContext &context) {
    if (node.children.size() < 2) {
        throw std::runtime_error("IfStmt requires condition and then statement");
    }

    generateExpression(node.children[0], symbols, context);
    const int falseJump = context.emit(OpCode::JPC, 0, 0, "if false");

    generateStatement(node.children[1], symbols, context);

    if (node.children.size() > 2) {
        const int endJump = context.emit(OpCode::JMP, 0, 0, "if end");
        context.patch(falseJump, context.nextInstructionIndex());
        generateStatement(node.children[2], symbols, context);
        context.patch(endJump, context.nextInstructionIndex());
        return;
    }

    context.patch(falseJump, context.nextInstructionIndex());
}

void generateWhileStatement(const AstNode &node, const SymbolTable &symbols,
                            CodeGenContext &context) {
    if (node.children.size() < 2) {
        throw std::runtime_error("WhileStmt requires condition and body");
    }

    const int start = context.nextInstructionIndex();
    generateExpression(node.children[0], symbols, context);
    const int exitJump = context.emit(OpCode::JPC, 0, 0, "while exit");
    generateStatement(node.children[1], symbols, context);
    context.emit(OpCode::JMP, 0, start, "while repeat");
    context.patch(exitJump, context.nextInstructionIndex());
}

void generateRepeatStatement(const AstNode &node, const SymbolTable &symbols,
                             CodeGenContext &context) {
    if (node.children.size() < 2) {
        throw std::runtime_error("RepeatStmt requires body and condition");
    }

    const int start = context.nextInstructionIndex();
    generateStatement(node.children[0], symbols, context);
    generateExpression(node.children[1], symbols, context);
    context.emit(OpCode::JPC, 0, start, "repeat until");
}

void generateForStatement(const AstNode &node, const SymbolTable &symbols,
                          CodeGenContext &context) {
    if (node.children.size() < 4) {
        throw std::runtime_error("ForStmt requires counter, bounds, and body");
    }

    const std::string direction = lowerCopy(node.text);
    const bool isTo = direction == "to";
    const bool isDownto = direction == "downto";
    if (!isTo && !isDownto) {
        throw std::runtime_error("Unsupported for direction: " + node.text);
    }

    const int counterSymbol =
        scalarAddressForVariable(node.children[0], symbols, context);

    generateExpression(node.children[1], symbols, context);
    context.emit(OpCode::STO, context.runtimeLevelOf(counterSymbol),
                 context.runtimeAddressOf(counterSymbol), "for init");

    const int start = context.nextInstructionIndex();
    context.emit(OpCode::LOD, context.runtimeLevelOf(counterSymbol),
                 context.runtimeAddressOf(counterSymbol), "for counter");
    generateExpression(node.children[2], symbols, context);
    emitOpr(context, isTo ? OprCode::LEQ : OprCode::GEQ,
            isTo ? "for to" : "for downto");
    const int exitJump = context.emit(OpCode::JPC, 0, 0, "for exit");

    generateStatement(node.children[3], symbols, context);

    context.emit(OpCode::LOD, context.runtimeLevelOf(counterSymbol),
                 context.runtimeAddressOf(counterSymbol), "for counter");
    context.emit(OpCode::LIT, 0, 1, "for step");
    emitOpr(context, isTo ? OprCode::ADD : OprCode::SUB,
            isTo ? "for increment" : "for decrement");
    context.emit(OpCode::STO, context.runtimeLevelOf(counterSymbol),
                 context.runtimeAddressOf(counterSymbol), "for update");
    context.emit(OpCode::JMP, 0, start, "for repeat");
    context.patch(exitJump, context.nextInstructionIndex());
}

void generateCaseStatement(const AstNode &node, const SymbolTable &symbols,
                           CodeGenContext &context) {
    if (node.children.empty()) {
        throw std::runtime_error("CaseStmt requires selector expression");
    }

    std::vector<int> endJumps;
    // Recheck the case expression for each case label because OPR EQL consumes both values.
    for (std::size_t branchIndex = 1; branchIndex < node.children.size();
         ++branchIndex) {
        const AstNode &branch = node.children[branchIndex];
        if (branch.children.size() < 2) {
            throw std::runtime_error("CaseBranch requires labels and statement");
        }

        const AstNode &labels = branch.children[0];
        for (const AstNode &label : labels.children) {
            generateExpression(node.children[0], symbols, context);
            generateExpression(label, symbols, context);
            emitOpr(context, OprCode::EQL, "case label");
            const int nextLabel = context.emit(OpCode::JPC, 0, 0, "case next");
            generateStatement(branch.children[1], symbols, context);
            endJumps.push_back(context.emit(OpCode::JMP, 0, 0, "case end"));
            context.patch(nextLabel, context.nextInstructionIndex());
        }
    }

    const int caseEnd = context.nextInstructionIndex();
    for (const int jump : endJumps) {
        context.patch(jump, caseEnd);
    }
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
    context.beginGlobalLayout();

    if (!decoratedAst.children.empty()) {
        allocateDeclarations(decoratedAst.children[0], symbols, context);
    }
    context.endGlobalLayout();

    int mainJump = -1;
    if (!decoratedAst.children.empty() &&
        hasSubprogramDeclarations(decoratedAst.children[0])) {
        mainJump = context.emit(OpCode::JMP, 0, 0, "main entry");
        generateDeclarations(decoratedAst.children[0], symbols, context);
        context.patch(mainJump, context.nextInstructionIndex());
    }

    context.emit(OpCode::INT, 0, context.globalFrameSize());

    if (decoratedAst.children.size() > 1) {
        generateStatement(decoratedAst.children[1], symbols, context);
    }

    context.emit(OpCode::RET, 0, -1);
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
        generateIfStatement(node, symbols, context);
        return;
    case AstKind::WhileStmt:
        generateWhileStatement(node, symbols, context);
        return;
    case AstKind::RepeatStmt:
        generateRepeatStatement(node, symbols, context);
        return;
    case AstKind::ForStmt:
        generateForStatement(node, symbols, context);
        return;
    case AstKind::CaseStmt:
        generateCaseStatement(node, symbols, context);
        return;
    case AstKind::Call:
        generateCall(node, symbols, context, false);
        return;
    default:
        unsupportedCodegenNode(node, "Akram/Endra");
    }
}
