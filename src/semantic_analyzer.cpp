#include "semantic_analyzer.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct TypeInfo {
    TypeKind kind = TypeKind::None;
    int ref = 0;
    TypeKind base = TypeKind::None;
    bool hasRange = false;
    int low = 0;
    int high = 0;
};

struct ExprInfo {
    TypeInfo type;
    bool hasIntValue = false;
    int intValue = 0;
    int symbolIndex = -1;
};

std::string lowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

bool parseInt(const std::string &text, int &value) {
    char *end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

std::string rangeLiteral(const TypeInfo &type) {
    if (!type.hasRange) {
        return "";
    }
    return std::to_string(type.low) + ".." + std::to_string(type.high);
}

TypeKind underlying(const TypeInfo &type) {
    return type.kind == TypeKind::Subrange ? type.base : type.kind;
}

bool isNumeric(TypeKind kind) {
    return kind == TypeKind::Integer || kind == TypeKind::Real ||
           kind == TypeKind::Subrange;
}

bool isScalar(TypeKind kind) {
    return kind == TypeKind::Integer || kind == TypeKind::Boolean ||
           kind == TypeKind::Char || kind == TypeKind::Subrange ||
           kind == TypeKind::Enum;
}

int typeSize(TypeKind kind) {
    return kind == TypeKind::Real ? 2 : (kind == TypeKind::None ? 0 : 1);
}

bool parseRangeLiteral(const std::string &literal, int &low, int &high) {
    const std::size_t dots = literal.find("..");
    if (dots == std::string::npos) {
        return false;
    }
    return parseInt(literal.substr(0, dots), low) &&
           parseInt(literal.substr(dots + 2), high);
}

TypeInfo typeFromEntry(const TabEntry &entry) {
    TypeInfo type;
    type.kind = entry.type;
    type.ref = entry.ref;
    if (entry.type == TypeKind::Subrange) {
        type.base = static_cast<TypeKind>(entry.ref);
        type.hasRange = parseRangeLiteral(entry.literalValue, type.low, type.high);
    } else if (entry.type == TypeKind::Enum) {
        type.hasRange = parseRangeLiteral(entry.literalValue, type.low, type.high);
    }
    return type;
}

class SemanticAnalyzer {
  public:
    explicit SemanticAnalyzer(AstNode ast) : ast_(std::move(ast)) {}

    SemanticResult analyze() {
        visitProgram(ast_);
        return {std::move(ast_), symbols_, diagnostics_};
    }

  private:
    AstNode ast_;
    SymbolTable symbols_;
    std::vector<SemanticDiagnostic> diagnostics_;
    int nextEnumRef_ = 1;
    int currentFunctionResultIndex_ = -1;

    void error(const std::string &message) {
        diagnostics_.push_back({SemanticDiagnostic::Severity::Error, message});
    }

    void warning(const std::string &message) {
        diagnostics_.push_back({SemanticDiagnostic::Severity::Warning, message});
    }

    void annotate(AstNode &node, const TypeInfo &type, int symbolIndex = -1) {
        node.inferredType = typeKindToString(type.kind);
        node.symbolIndex = symbolIndex;
        node.lexicalLevel = symbols_.currentLevel();
    }

    int insertSymbol(const std::string &name, ObjectKind obj, const TypeInfo &type,
                     bool initialized = false, const std::string &literal = "") {
        try {
            return symbols_.insert(name, obj, type.kind, type.ref, true, 0,
                                   initialized, literal);
        } catch (const std::exception &e) {
            error(e.what());
            return -1;
        }
    }

    void visitProgram(AstNode &node) {
        TypeInfo none;
        const int programIndex =
            insertSymbol(node.text, ObjectKind::Program, none, true);
        annotate(node, none, programIndex);

        if (!node.children.empty()) {
            visitDeclarations(node.children[0]);
        }
        if (node.children.size() > 1) {
            symbols_.pushScope(BlockKind::Compound);
            visitStatement(node.children[1]);
            symbols_.popScope();
        }
    }

    void visitDeclarations(AstNode &node) {
        annotate(node, TypeInfo{});
        for (AstNode &child : node.children) {
            switch (child.kind) {
            case AstKind::ConstDecl:
                visitConstDecl(child);
                break;
            case AstKind::TypeDecl:
                visitTypeDecl(child);
                break;
            case AstKind::VarDecl:
                visitVarDecl(child);
                break;
            case AstKind::ProcedureDecl:
                visitProcedureDecl(child);
                break;
            case AstKind::FunctionDecl:
                visitFunctionDecl(child);
                break;
            default:
                break;
            }
        }
    }

    void visitConstDecl(AstNode &node) {
        ExprInfo value = node.children.empty() ? ExprInfo{} : evalConstant(node.children[0]);
        const std::string literal =
            node.children.empty() ? "" : constantLiteral(node.children[0]);
        const int index =
            insertSymbol(node.text, ObjectKind::Constant, value.type, true, literal);
        annotate(node, value.type, index);
    }

    void visitTypeDecl(AstNode &node) {
        TypeInfo type = node.children.empty() ? TypeInfo{} : resolveType(node.children[0]);
        const int index =
            insertSymbol(node.text, ObjectKind::Type, type, true, rangeLiteral(type));
        annotate(node, type, index);
    }

    void visitVarDecl(AstNode &node) {
        TypeInfo type = node.children.empty() ? TypeInfo{} : resolveType(node.children[0]);
        const int index =
            insertSymbol(node.text, ObjectKind::Variable, type, false, rangeLiteral(type));
        annotate(node, type, index);
    }

    void visitProcedureDecl(AstNode &node) {
        TypeInfo none;
        const int outerIndex =
            insertSymbol(node.text, ObjectKind::Procedure, none, true);
        annotate(node, none, outerIndex);

        const int blockIndex = symbols_.pushScope(BlockKind::Procedure);
        if (outerIndex >= 0) {
            symbols_.setReference(outerIndex, blockIndex);
        }

        if (!node.children.empty()) {
            visitParameters(node.children[0]);
        }
        if (node.children.size() > 1) {
            visitDeclarations(node.children[1]);
        }
        if (node.children.size() > 2) {
            visitStatement(node.children[2]);
        }
        symbols_.popScope();
    }

    void visitFunctionDecl(AstNode &node) {
        TypeInfo returnType =
            node.children.size() > 1 ? resolveType(node.children[1]) : TypeInfo{};
        const int outerIndex =
            insertSymbol(node.text, ObjectKind::Function, returnType, true);
        annotate(node, returnType, outerIndex);

        const int blockIndex = symbols_.pushScope(BlockKind::Function);
        if (outerIndex >= 0) {
            symbols_.setReference(outerIndex, blockIndex);
        }
        if (!node.children.empty()) {
            visitParameters(node.children[0]);
        }
        if (node.children.size() > 2) {
            visitDeclarations(node.children[2]);
        }
        if (node.children.size() > 3) {
            const int previousFunctionResultIndex = currentFunctionResultIndex_;
            currentFunctionResultIndex_ = outerIndex;
            visitStatement(node.children[3]);
            currentFunctionResultIndex_ = previousFunctionResultIndex;
        }
        symbols_.popScope();
    }

    void visitParameters(AstNode &node) {
        annotate(node, TypeInfo{});
        for (AstNode &param : node.children) {
            TypeInfo type =
                param.children.empty() ? TypeInfo{} : resolveType(param.children[0]);
            const int index =
                insertSymbol(param.text, ObjectKind::Parameter, type, true,
                             rangeLiteral(type));
            annotate(param, type, index);
        }
    }

    TypeInfo resolveType(AstNode &node) {
        switch (node.kind) {
        case AstKind::NamedType:
            return resolveNamedType(node);
        case AstKind::RangeType:
            return resolveRangeType(node);
        case AstKind::ArrayType:
            return resolveArrayType(node);
        case AstKind::RecordType:
            return resolveRecordType(node);
        case AstKind::EnumType:
            return resolveEnumType(node);
        default:
            error("Invalid type node: " + astKindToString(node.kind));
            annotate(node, TypeInfo{});
            return {};
        }
    }

    TypeInfo resolveNamedType(AstNode &node) {
        const int index = symbols_.lookup(node.text);
        if (index < 0) {
            error("Undeclared type: " + node.text);
            annotate(node, TypeInfo{});
            return {};
        }

        const TabEntry &entry = symbols_.tabEntry(index);
        if (entry.obj != ObjectKind::Type) {
            error("Identifier is not a type: " + node.text);
            annotate(node, TypeInfo{}, index);
            return {};
        }

        TypeInfo type = typeFromEntry(entry);
        annotate(node, type, index);
        return type;
    }

    TypeInfo resolveRangeType(AstNode &node) {
        ExprInfo low = node.children.size() > 0 ? evalConstant(node.children[0]) : ExprInfo{};
        ExprInfo high = node.children.size() > 1 ? evalConstant(node.children[1]) : ExprInfo{};

        TypeInfo type;
        type.kind = TypeKind::Subrange;
        type.base = underlying(low.type);
        type.ref = static_cast<int>(type.base);
        type.hasRange = low.hasIntValue && high.hasIntValue;
        type.low = low.intValue;
        type.high = high.intValue;

        if (type.base == TypeKind::Real) {
            error("Subrange type cannot be Real");
        }
        if (!sameBaseType(low.type, high.type)) {
            error("Subrange bounds must have the same type");
        }
        if (type.hasRange && type.low > type.high) {
            error("Subrange lower bound cannot exceed upper bound");
        }

        annotate(node, type);
        return type;
    }

    TypeInfo resolveArrayType(AstNode &node) {
        TypeInfo indexType =
            node.children.size() > 0 ? resolveType(node.children[0]) : TypeInfo{};
        TypeInfo elementType =
            node.children.size() > 1 ? resolveType(node.children[1]) : TypeInfo{};

        if (underlying(indexType) == TypeKind::Real) {
            error("Array index type cannot be Real");
        }

        int ref = 0;
        try {
            ref = symbols_.insertArray(underlying(indexType), elementType.kind,
                                       elementType.ref,
                                       indexType.hasRange ? indexType.low : 0,
                                       indexType.hasRange ? indexType.high : 0,
                                       typeSize(elementType.kind));
        } catch (const std::exception &e) {
            error(e.what());
        }

        TypeInfo type;
        type.kind = TypeKind::Array;
        type.ref = ref;
        annotate(node, type);
        return type;
    }

    TypeInfo resolveRecordType(AstNode &node) {
        const int blockIndex = symbols_.pushScope(BlockKind::Record);
        for (AstNode &field : node.children) {
            TypeInfo fieldType =
                field.children.empty() ? TypeInfo{} : resolveType(field.children[0]);
            const int fieldIndex = insertSymbol(field.text, ObjectKind::Field,
                                                fieldType, true,
                                                rangeLiteral(fieldType));
            annotate(field, fieldType, fieldIndex);
        }
        symbols_.popScope();

        TypeInfo type;
        type.kind = TypeKind::Record;
        type.ref = blockIndex;
        annotate(node, type);
        return type;
    }

    TypeInfo resolveEnumType(AstNode &node) {
        TypeInfo type;
        type.kind = TypeKind::Enum;
        type.ref = nextEnumRef_++;
        type.hasRange = !node.children.empty();
        type.low = 0;
        type.high = static_cast<int>(node.children.size()) - 1;

        int ordinal = 0;
        for (AstNode &ident : node.children) {
            const int index =
                insertSymbol(ident.text, ObjectKind::Constant, type, true,
                             std::to_string(ordinal));
            annotate(ident, type, index);
            ++ordinal;
        }

        annotate(node, type);
        return type;
    }

    void visitStatement(AstNode &node) {
        node.lexicalLevel = symbols_.currentLevel();
        switch (node.kind) {
        case AstKind::CompoundStmt:
        case AstKind::StatementList:
            for (AstNode &child : node.children) {
                visitStatement(child);
            }
            annotate(node, TypeInfo{});
            break;
        case AstKind::EmptyStmt:
            annotate(node, TypeInfo{});
            break;
        case AstKind::AssignStmt:
            visitAssignment(node);
            break;
        case AstKind::IfStmt:
            visitConditional(node, "if");
            break;
        case AstKind::WhileStmt:
            visitLoop(node, "while");
            break;
        case AstKind::RepeatStmt:
            visitRepeat(node);
            break;
        case AstKind::ForStmt:
            visitFor(node);
            break;
        case AstKind::CaseStmt:
            visitCase(node);
            break;
        case AstKind::Call:
            evalCall(node, true);
            break;
        case AstKind::Identifier:
            visitBareIdentifierStatement(node);
            break;
        default:
            evalExpression(node);
            break;
        }
    }

    void visitBareIdentifierStatement(AstNode &node) {
        TypeInfo none;
        const int index = symbols_.lookup(node.text);
        if (index < 0) {
            error("Identifier is not a valid statement/callable: " + node.text);
            annotate(node, none);
            return;
        }

        const TabEntry &entry = symbols_.tabEntry(index);
        TypeInfo type = typeFromEntry(entry);
        annotate(node, type, index);

        if (entry.obj != ObjectKind::Procedure && entry.obj != ObjectKind::Function) {
            error("Identifier is not a valid statement/callable: " + node.text);
            return;
        }

        const std::string normalized = SymbolTable::normalizeIdentifier(node.text);
        if (normalized != "readln" && normalized != "writeln" && entry.ref > 0) {
            std::vector<ExprInfo> args;
            checkCallArguments(entry, args);
        }
    }

    void visitAssignment(AstNode &node) {
        ExprInfo target =
            node.children.empty() ? ExprInfo{} : evalVariable(node.children[0], true);
        ExprInfo value =
            node.children.size() > 1 ? evalExpression(node.children[1]) : ExprInfo{};
        if (!assignmentCompatible(target.type, value)) {
            error("Incompatible assignment: cannot assign " +
                  typeKindToString(value.type.kind) + " to " +
                  typeKindToString(target.type.kind));
        }
        if (target.symbolIndex >= 0) {
            try {
                symbols_.markInitialized(target.symbolIndex);
            } catch (const std::exception &e) {
                error(e.what());
            }
        }
        annotate(node, TypeInfo{});
    }

    void visitConditional(AstNode &node, const std::string &name) {
        if (!node.children.empty()) {
            ExprInfo condition = evalExpression(node.children[0]);
            requireBoolean(condition.type, name + " condition");
        }
        for (std::size_t i = 1; i < node.children.size(); ++i) {
            visitStatement(node.children[i]);
        }
        annotate(node, TypeInfo{});
    }

    void visitLoop(AstNode &node, const std::string &name) {
        if (!node.children.empty()) {
            ExprInfo condition = evalExpression(node.children[0]);
            requireBoolean(condition.type, name + " condition");
        }
        if (node.children.size() > 1) {
            visitStatement(node.children[1]);
        }
        annotate(node, TypeInfo{});
    }

    void visitRepeat(AstNode &node) {
        if (!node.children.empty()) {
            visitStatement(node.children[0]);
        }
        if (node.children.size() > 1) {
            ExprInfo condition = evalExpression(node.children[1]);
            requireBoolean(condition.type, "repeat condition");
        }
        annotate(node, TypeInfo{});
    }

    void visitFor(AstNode &node) {
        if (node.children.empty()) {
            annotate(node, TypeInfo{});
            return;
        }

        ExprInfo counter = evalIdentifier(node.children[0], true);
        if (underlying(counter.type) != TypeKind::Integer) {
            error("For counter must be integer-compatible");
        }
        if (node.children.size() > 1) {
            ExprInfo start = evalExpression(node.children[1]);
            if (!assignmentCompatible(counter.type, start)) {
                error("For initial value is not assignment-compatible with counter");
            }
        }
        if (node.children.size() > 2) {
            ExprInfo stop = evalExpression(node.children[2]);
            if (!assignmentCompatible(counter.type, stop)) {
                error("For final value is not assignment-compatible with counter");
            }
        }
        if (counter.symbolIndex >= 0) {
            symbols_.markInitialized(counter.symbolIndex);
        }
        if (node.children.size() > 3) {
            visitStatement(node.children[3]);
        }
        annotate(node, TypeInfo{});
    }

    void visitCase(AstNode &node) {
        ExprInfo selector =
            node.children.empty() ? ExprInfo{} : evalExpression(node.children[0]);
        for (std::size_t i = 1; i < node.children.size(); ++i) {
            AstNode &branch = node.children[i];
            if (branch.children.size() > 0) {
                for (AstNode &label : branch.children[0].children) {
                    ExprInfo labelValue = evalConstant(label);
                    if (!typeCompatible(selector.type, labelValue.type)) {
                        error("Case label type is incompatible with selector");
                    }
                }
            }
            if (branch.children.size() > 1) {
                visitStatement(branch.children[1]);
            }
            annotate(branch, TypeInfo{});
        }
        annotate(node, TypeInfo{});
    }

    ExprInfo evalExpression(AstNode &node) {
        switch (node.kind) {
        case AstKind::Literal:
            return evalLiteral(node);
        case AstKind::Identifier:
            return evalIdentifier(node, false);
        case AstKind::Variable:
            return evalVariable(node, false);
        case AstKind::UnaryExpr:
            return evalUnary(node);
        case AstKind::BinaryExpr:
            return evalBinary(node);
        case AstKind::Call:
            return evalCall(node, false);
        default:
            annotate(node, TypeInfo{});
            return {};
        }
    }

    ExprInfo evalConstant(AstNode &node) {
        ExprInfo value = evalExpression(node);
        if (node.kind == AstKind::Identifier) {
            const int index = symbols_.lookup(node.text);
            if (index >= 0 && symbols_.tabEntry(index).obj != ObjectKind::Constant) {
                error("Identifier is not a constant: " + node.text);
            }
        }
        return value;
    }

    ExprInfo evalLiteral(AstNode &node) {
        ExprInfo info;
        int intValue = 0;
        if (parseInt(node.text, intValue)) {
            info.type.kind = TypeKind::Integer;
            info.hasIntValue = true;
            info.intValue = intValue;
        } else if (node.text.find('.') != std::string::npos &&
                   node.text.find('\'') == std::string::npos) {
            info.type.kind = TypeKind::Real;
        } else if (node.text.size() >= 2 && node.text.front() == '\'' &&
                   node.text.back() == '\'') {
            if (node.text.size() == 3) {
                info.type.kind = TypeKind::Char;
                info.hasIntValue = true;
                info.intValue = static_cast<unsigned char>(node.text[1]);
            } else {
                info.type.kind = TypeKind::String;
            }
        } else {
            info.type.kind = TypeKind::None;
        }
        annotate(node, info.type);
        return info;
    }

    ExprInfo evalIdentifier(AstNode &node, bool asLValue) {
        ExprInfo info;
        const int index = symbols_.lookup(node.text);
        if (index < 0) {
            error("Undeclared identifier: " + node.text);
            annotate(node, info.type);
            return info;
        }

        const TabEntry &entry = symbols_.tabEntry(index);
        info.type = typeFromEntry(entry);
        info.symbolIndex = index;
        if (entry.obj == ObjectKind::Constant) {
            parseConstantEntryValue(entry, info);
        }
        if (!asLValue && entry.obj == ObjectKind::Procedure) {
            error("Procedure has no return value: " + node.text);
        }
        if (!asLValue && entry.obj == ObjectKind::Function && entry.ref > 0) {
            std::vector<ExprInfo> args;
            checkCallArguments(entry, args);
        }
        if (!asLValue && entry.obj == ObjectKind::Variable && !entry.initialized) {
            warning("Variable may be used before initialization: " + node.text);
        }
        const bool isCurrentFunctionResult =
            entry.obj == ObjectKind::Function && index == currentFunctionResultIndex_;
        if (asLValue && entry.obj != ObjectKind::Variable &&
            entry.obj != ObjectKind::Parameter && entry.obj != ObjectKind::Field &&
            !isCurrentFunctionResult) {
            error("Identifier is not assignable: " + node.text);
        }
        if (!asLValue &&
            (entry.obj == ObjectKind::Program || entry.obj == ObjectKind::Type ||
             entry.obj == ObjectKind::Reserved)) {
            error("Identifier is not a value: " + node.text);
        }

        annotate(node, info.type, index);
        return info;
    }

    ExprInfo evalVariable(AstNode &node, bool asLValue) {
        ExprInfo info = evalIdentifier(node, asLValue);
        TypeInfo current = info.type;

        for (AstNode &component : node.children) {
            if (component.kind == AstKind::IndexAccess) {
                current = evalIndexAccess(component, current);
            } else if (component.kind == AstKind::FieldAccess) {
                current = evalFieldAccess(component, current);
            }
        }

        info.type = current;
        annotate(node, current, info.symbolIndex);
        return info;
    }

    TypeInfo evalIndexAccess(AstNode &node, const TypeInfo &arrayType) {
        TypeInfo current = arrayType;
        for (AstNode &indexNode : node.children) {
            if (current.kind != TypeKind::Array) {
                error("Array index used on non-array type");
                evalExpression(indexNode);
                current = {};
                continue;
            }

            const ATabEntry &array = symbols_.atabEntry(current.ref);
            ExprInfo index = evalExpression(indexNode);
            TypeInfo expectedIndex;
            expectedIndex.kind = array.xtyp;
            if (!typeCompatible(expectedIndex, index.type)) {
                error("Invalid array index type: expected " +
                      typeKindToString(array.xtyp) + ", got " +
                      typeKindToString(index.type.kind));
            }
            if (index.hasIntValue && !(array.low == 0 && array.high == 0) &&
                (index.intValue < array.low || index.intValue > array.high)) {
                error("Array index literal is outside declared bounds");
            }

            current.kind = array.etyp;
            current.ref = array.eref;
        }
        annotate(node, current);
        return current;
    }

    TypeInfo evalFieldAccess(AstNode &node, const TypeInfo &recordType) {
        if (recordType.kind != TypeKind::Record) {
            error("Field access used on non-record type: " + node.text);
            annotate(node, TypeInfo{});
            return {};
        }

        int index = symbols_.btabEntry(recordType.ref).last;
        const std::string wanted = SymbolTable::normalizeIdentifier(node.text);
        while (index > 0) {
            const TabEntry &entry = symbols_.tabEntry(index);
            if (entry.identifier == wanted && entry.obj == ObjectKind::Field) {
                TypeInfo type = typeFromEntry(entry);
                annotate(node, type, index);
                return type;
            }
            index = entry.link;
        }

        error("Invalid record field: " + node.text);
        annotate(node, TypeInfo{});
        return {};
    }

    ExprInfo evalUnary(AstNode &node) {
        ExprInfo operand =
            node.children.empty() ? ExprInfo{} : evalExpression(node.children[0]);
        ExprInfo result;
        const std::string op = lowerCopy(node.text);

        if (op == "notsy") {
            requireBoolean(operand.type, "not operand");
            result.type.kind = TypeKind::Boolean;
        } else if (op == "plus" || op == "minus") {
            if (!isNumeric(underlying(operand.type))) {
                error("Invalid unary operator operand: " + node.text);
            }
            result.type.kind = underlying(operand.type);
            if (operand.hasIntValue) {
                result.hasIntValue = true;
                result.intValue = op == "minus" ? -operand.intValue : operand.intValue;
            }
        } else {
            error("Unknown unary operator: " + node.text);
        }

        annotate(node, result.type);
        return result;
    }

    ExprInfo evalBinary(AstNode &node) {
        ExprInfo left =
            node.children.size() > 0 ? evalExpression(node.children[0]) : ExprInfo{};
        ExprInfo right =
            node.children.size() > 1 ? evalExpression(node.children[1]) : ExprInfo{};
        ExprInfo result;
        const std::string op = lowerCopy(node.text);

        if (op == "plus" || op == "minus" || op == "times") {
            result.type = arithmeticResult(left.type, right.type, op);
        } else if (op == "rdiv") {
            if (!isNumeric(underlying(left.type)) || !isNumeric(underlying(right.type))) {
                error("Operator / requires numeric operands");
            }
            result.type.kind = TypeKind::Real;
        } else if (op == "idiv" || op == "imod") {
            if (underlying(left.type) != TypeKind::Integer ||
                underlying(right.type) != TypeKind::Integer) {
                error("Operator " + node.text + " requires integer operands");
            }
            result.type.kind = TypeKind::Integer;
        } else if (op == "andsy" || op == "orsy") {
            requireBoolean(left.type, node.text + " left operand");
            requireBoolean(right.type, node.text + " right operand");
            result.type.kind = TypeKind::Boolean;
        } else {
            if (!typeCompatible(left.type, right.type) &&
                !(isNumeric(underlying(left.type)) && isNumeric(underlying(right.type)))) {
                error("Relational operands are not compatible");
            }
            result.type.kind = TypeKind::Boolean;
        }

        annotate(node, result.type);
        return result;
    }

    ExprInfo evalCall(AstNode &node, bool asStatement) {
        ExprInfo result;
        const int index = symbols_.lookup(node.text);
        if (index < 0) {
            error("Undeclared procedure/function: " + node.text);
            annotate(node, result.type);
            for (AstNode &arg : node.children) {
                evalExpression(arg);
            }
            return result;
        }

        const TabEntry &entry = symbols_.tabEntry(index);
        const bool isCallable =
            entry.obj == ObjectKind::Procedure || entry.obj == ObjectKind::Function;
        if (!isCallable) {
            error("Identifier is not callable: " + node.text);
        }
        if (!asStatement && entry.obj == ObjectKind::Procedure) {
            error("Procedure has no return value: " + node.text);
        }

        std::vector<ExprInfo> args;
        for (AstNode &arg : node.children) {
            args.push_back(evalExpression(arg));
        }

        const std::string normalized = SymbolTable::normalizeIdentifier(node.text);
        if (isCallable && normalized != "readln" && normalized != "writeln" &&
            entry.ref > 0) {
            checkCallArguments(entry, args);
        }

        result.type = typeFromEntry(entry);
        result.symbolIndex = index;
        annotate(node, result.type, index);
        return result;
    }

    void checkCallArguments(const TabEntry &callable,
                            const std::vector<ExprInfo> &args) {
        std::vector<TypeInfo> params;
        int paramIndex = symbols_.btabEntry(callable.ref).lpar;
        while (paramIndex > 0) {
            const TabEntry &param = symbols_.tabEntry(paramIndex);
            if (param.obj != ObjectKind::Parameter) {
                break;
            }
            params.push_back(typeFromEntry(param));
            paramIndex = param.link;
        }
        std::reverse(params.begin(), params.end());

        if (params.size() != args.size()) {
            error("Call argument count mismatch for " + callable.identifier);
            return;
        }
        for (std::size_t i = 0; i < params.size(); ++i) {
            if (!assignmentCompatible(params[i], args[i])) {
                error("Call argument " + std::to_string(i + 1) +
                      " is not assignment-compatible for " + callable.identifier);
            }
        }
    }

    TypeInfo arithmeticResult(const TypeInfo &left, const TypeInfo &right,
                              const std::string &op) {
        TypeInfo result;
        if (!isNumeric(underlying(left)) || !isNumeric(underlying(right))) {
            error("Operator " + op + " requires numeric operands");
            return result;
        }
        result.kind = underlying(left) == TypeKind::Real ||
                              underlying(right) == TypeKind::Real
                          ? TypeKind::Real
                          : TypeKind::Integer;
        return result;
    }

    bool sameBaseType(const TypeInfo &left, const TypeInfo &right) const {
        const TypeKind a = underlying(left);
        const TypeKind b = underlying(right);
        return a == b || left.kind == TypeKind::None || right.kind == TypeKind::None;
    }

    bool typeCompatible(const TypeInfo &left, const TypeInfo &right) const {
        if (left.kind == TypeKind::None || right.kind == TypeKind::None) {
            return true;
        }
        if (left.kind == right.kind) {
            if (left.kind == TypeKind::Array || left.kind == TypeKind::Record ||
                left.kind == TypeKind::Enum) {
                return left.ref == right.ref;
            }
            return true;
        }
        if (left.kind == TypeKind::Subrange || right.kind == TypeKind::Subrange) {
            return underlying(left) == underlying(right);
        }
        if (left.kind == TypeKind::String && right.kind == TypeKind::String) {
            return true;
        }
        return false;
    }

    bool assignmentCompatible(const TypeInfo &target, const ExprInfo &value) const {
        return assignmentCompatible(target, value.type, value.hasIntValue,
                                    value.intValue);
    }

    bool assignmentCompatible(const TypeInfo &target, const TypeInfo &value) const {
        return assignmentCompatible(target, value, false, 0);
    }

    bool assignmentCompatible(const TypeInfo &target, const TypeInfo &value,
                              bool hasIntValue, int intValue) const {
        if (target.kind == TypeKind::None || value.kind == TypeKind::None) {
            return true;
        }
        if (target.kind == TypeKind::Real && underlying(value) == TypeKind::Integer) {
            return true;
        }
        if (target.kind == TypeKind::Subrange && hasIntValue &&
            (intValue < target.low || intValue > target.high)) {
            return false;
        }
        if (typeCompatible(target, value) &&
            (isScalar(target.kind) || target.kind == TypeKind::String ||
             target.kind == TypeKind::Array || target.kind == TypeKind::Record)) {
            return true;
        }
        return false;
    }

    void requireBoolean(const TypeInfo &type, const std::string &context) {
        if (type.kind != TypeKind::None && underlying(type) != TypeKind::Boolean) {
            error(context + " must be boolean");
        }
    }

    std::string constantLiteral(const AstNode &node) const {
        if (node.kind == AstKind::Literal || node.kind == AstKind::Identifier) {
            return node.text;
        }
        if (node.kind == AstKind::UnaryExpr && !node.children.empty()) {
            return (lowerCopy(node.text) == "minus" ? "-" : "") +
                   constantLiteral(node.children[0]);
        }
        return "";
    }

    void parseConstantEntryValue(const TabEntry &entry, ExprInfo &info) const {
        const std::string literal = lowerCopy(entry.literalValue);
        if (literal == "true") {
            info.hasIntValue = true;
            info.intValue = 1;
        } else if (literal == "false") {
            info.hasIntValue = true;
            info.intValue = 0;
        } else {
            int intValue = 0;
            if (parseInt(entry.literalValue, intValue)) {
                info.hasIntValue = true;
                info.intValue = intValue;
            }
        }
    }
};

} // namespace

bool SemanticResult::ok() const {
    for (const SemanticDiagnostic &diagnostic : diagnostics) {
        if (diagnostic.severity == SemanticDiagnostic::Severity::Error) {
            return false;
        }
    }
    return true;
}

SemanticResult analyzeSemantics(AstNode ast) {
    SemanticAnalyzer analyzer(std::move(ast));
    return analyzer.analyze();
}
