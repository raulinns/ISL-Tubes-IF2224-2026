#include "interpreter.h"

#include "arion_runtime_error.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <istream>
#include <string>
#include <utility>

namespace {

std::string lowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::string operatorHint(const Instruction &instruction) {
    return lowerCopy(instruction.comment);
}

RuntimeValue popOperand(Interpreter &interpreter, const std::string &context) {
    try {
        return interpreter.stack().popValue();
    } catch (const StackUnderflowError &) {
        throw StackUnderflowError("Runtime stack underflow during " + context);
    }
}

std::pair<RuntimeValue, RuntimeValue>
popBinaryOperands(Interpreter &interpreter, const std::string &context) {
    RuntimeValue right = popOperand(interpreter, context);
    RuntimeValue left = popOperand(interpreter, context);
    return {std::move(left), std::move(right)};
}

bool isZero(const RuntimeValue &value) {
    switch (value.kind()) {
    case RuntimeValueKind::Integer:
        return value.asInteger() == 0;
    case RuntimeValueKind::Real:
        return value.asReal() == 0.0;
    default:
        return false;
    }
}

bool isNumeric(const RuntimeValue &value) { return value.isNumeric(); }

bool isBoolean(const RuntimeValue &value) {
    return value.kind() == RuntimeValueKind::Boolean;
}

bool asBoolean(const RuntimeValue &value, const std::string &context) {
    if (!isBoolean(value)) {
        throw RuntimeTypeError(context + " requires boolean operands");
    }
    return value.asBoolean();
}

int asInteger(const RuntimeValue &value, const std::string &context) {
    if (value.kind() != RuntimeValueKind::Integer) {
        throw RuntimeTypeError(context + " requires integer operands");
    }
    return value.asInteger();
}

RuntimeValue addValues(const RuntimeValue &left, const RuntimeValue &right,
                       const std::string &hint) {
    if (hint == "orsy") {
        return RuntimeValue::makeBoolean(asBoolean(left, "Logical or") ||
                                         asBoolean(right, "Logical or"));
    }

    if (!isNumeric(left) || !isNumeric(right)) {
        throw RuntimeTypeError("ADD requires numeric operands");
    }
    if (left.kind() == RuntimeValueKind::Integer &&
        right.kind() == RuntimeValueKind::Integer) {
        return RuntimeValue::makeInteger(left.asInteger() + right.asInteger());
    }
    return RuntimeValue::makeReal(left.asReal() + right.asReal());
}

RuntimeValue subtractValues(const RuntimeValue &left, const RuntimeValue &right) {
    if (!isNumeric(left) || !isNumeric(right)) {
        throw RuntimeTypeError("SUB requires numeric operands");
    }
    if (left.kind() == RuntimeValueKind::Integer &&
        right.kind() == RuntimeValueKind::Integer) {
        return RuntimeValue::makeInteger(left.asInteger() - right.asInteger());
    }
    return RuntimeValue::makeReal(left.asReal() - right.asReal());
}

RuntimeValue multiplyValues(const RuntimeValue &left, const RuntimeValue &right,
                            const std::string &hint) {
    if (hint == "andsy") {
        return RuntimeValue::makeBoolean(asBoolean(left, "Logical and") &&
                                         asBoolean(right, "Logical and"));
    }

    if (!isNumeric(left) || !isNumeric(right)) {
        throw RuntimeTypeError("MUL requires numeric operands");
    }
    if (left.kind() == RuntimeValueKind::Integer &&
        right.kind() == RuntimeValueKind::Integer) {
        return RuntimeValue::makeInteger(left.asInteger() * right.asInteger());
    }
    return RuntimeValue::makeReal(left.asReal() * right.asReal());
}

RuntimeValue divideValues(const RuntimeValue &left, const RuntimeValue &right,
                          const std::string &hint) {
    if (!isNumeric(left) || !isNumeric(right)) {
        throw RuntimeTypeError("DIV requires numeric operands");
    }
    if (isZero(right)) {
        throw DivisionByZeroError("Division by zero");
    }
    if (hint == "idiv") {
        return RuntimeValue::makeInteger(asInteger(left, "Integer division") /
                                         asInteger(right, "Integer division"));
    }
    if (hint == "rdiv") {
        return RuntimeValue::makeReal(left.asReal() / right.asReal());
    }
    if (left.kind() == RuntimeValueKind::Integer &&
        right.kind() == RuntimeValueKind::Integer) {
        return RuntimeValue::makeInteger(left.asInteger() / right.asInteger());
    }
    return RuntimeValue::makeReal(left.asReal() / right.asReal());
}

RuntimeValue moduloValues(const RuntimeValue &left, const RuntimeValue &right) {
    const int divisor = asInteger(right, "Modulo");
    if (divisor == 0) {
        throw DivisionByZeroError("Modulo by zero");
    }
    return RuntimeValue::makeInteger(asInteger(left, "Modulo") % divisor);
}

RuntimeValue inputValueFromToken(const std::string &token,
                                 const std::string &declaredType) {
    const std::string type = lowerCopy(declaredType);
    if (type == "string") {
        return RuntimeValue::makeString(token);
    }
    if (type == "char" && token.size() == 1U) {
        return RuntimeValue::makeChar(token[0]);
    }
    return RuntimeValue::parseLiteral(token, declaredType);
}

void executeReadln(Interpreter &interpreter, const Instruction &instruction) {
    std::string token;
    if (!(interpreter.input() >> token)) {
        throw ArionRuntimeError("readln expected an input value");
    }

    try {
        interpreter.stack().writeAt(
            instruction.level,
            inputValueFromToken(token, instruction.literalText));
    } catch (const std::exception &e) {
        throw RuntimeTypeError("readln cannot parse input '" + token +
                               "' as " + instruction.literalText + ": " +
                               e.what());
    }

    interpreter.advance();
}

int compareValues(const RuntimeValue &left, const RuntimeValue &right,
                  const std::string &context) {
    if (isNumeric(left) && isNumeric(right)) {
        const double lhs = left.asReal();
        const double rhs = right.asReal();
        if (lhs < rhs) {
            return -1;
        }
        if (lhs > rhs) {
            return 1;
        }
        return 0;
    }

    if (left.kind() != right.kind()) {
        throw RuntimeTypeError(context + " requires compatible operand types");
    }

    switch (left.kind()) {
    case RuntimeValueKind::Boolean: {
        const bool lhs = left.asBoolean();
        const bool rhs = right.asBoolean();
        if (lhs == rhs) {
            return 0;
        }
        return lhs ? 1 : -1;
    }
    case RuntimeValueKind::Char: {
        const char lhs = left.asChar();
        const char rhs = right.asChar();
        if (lhs < rhs) {
            return -1;
        }
        if (lhs > rhs) {
            return 1;
        }
        return 0;
    }
    case RuntimeValueKind::String: {
        const std::string &lhs = left.asString();
        const std::string &rhs = right.asString();
        if (lhs < rhs) {
            return -1;
        }
        if (lhs > rhs) {
            return 1;
        }
        return 0;
    }
    case RuntimeValueKind::Integer:
    case RuntimeValueKind::Real:
        break;
    case RuntimeValueKind::Uninitialized:
        throw RuntimeTypeError(context + " does not accept uninitialized values");
    }

    throw RuntimeTypeError(context + " requires comparable operands");
}

} // namespace

void executeOprInstruction(Interpreter &interpreter,
                           const Instruction &instruction) {
    const std::optional<OprCode> decoded = decodeOprCode(instruction.arg);
    if (!decoded.has_value()) {
        throw ArionRuntimeError("Unknown OPR code: " +
                                std::to_string(instruction.arg));
    }

    const std::string hint = operatorHint(instruction);

    switch (*decoded) {
    case OprCode::NEG: {
        const RuntimeValue operand = popOperand(interpreter, "NEG");
        if (operand.kind() == RuntimeValueKind::Integer) {
            interpreter.stack().pushValue(
                RuntimeValue::makeInteger(-operand.asInteger()));
        } else if (operand.kind() == RuntimeValueKind::Real) {
            interpreter.stack().pushValue(RuntimeValue::makeReal(-operand.asReal()));
        } else {
            throw RuntimeTypeError("NEG requires numeric operand");
        }
        interpreter.advance();
        return;
    }
    case OprCode::ADD: {
        const auto [left, right] = popBinaryOperands(interpreter, "ADD");
        interpreter.stack().pushValue(addValues(left, right, hint));
        interpreter.advance();
        return;
    }
    case OprCode::SUB: {
        const auto [left, right] = popBinaryOperands(interpreter, "SUB");
        interpreter.stack().pushValue(subtractValues(left, right));
        interpreter.advance();
        return;
    }
    case OprCode::MUL: {
        const auto [left, right] = popBinaryOperands(interpreter, "MUL");
        interpreter.stack().pushValue(multiplyValues(left, right, hint));
        interpreter.advance();
        return;
    }
    case OprCode::DIV: {
        const auto [left, right] = popBinaryOperands(interpreter, "DIV");
        interpreter.stack().pushValue(divideValues(left, right, hint));
        interpreter.advance();
        return;
    }
    case OprCode::MOD: {
        const auto [left, right] = popBinaryOperands(interpreter, "MOD");
        interpreter.stack().pushValue(moduloValues(left, right));
        interpreter.advance();
        return;
    }
    case OprCode::EQL: {
        const auto [left, right] = popBinaryOperands(interpreter, "EQL");
        interpreter.stack().pushValue(
            RuntimeValue::makeBoolean(compareValues(left, right, "EQL") == 0));
        interpreter.advance();
        return;
    }
    case OprCode::NEQ: {
        const auto [left, right] = popBinaryOperands(interpreter, "NEQ");
        interpreter.stack().pushValue(
            RuntimeValue::makeBoolean(compareValues(left, right, "NEQ") != 0));
        interpreter.advance();
        return;
    }
    case OprCode::LSS: {
        const auto [left, right] = popBinaryOperands(interpreter, "LSS");
        interpreter.stack().pushValue(
            RuntimeValue::makeBoolean(compareValues(left, right, "LSS") < 0));
        interpreter.advance();
        return;
    }
    case OprCode::GEQ: {
        const auto [left, right] = popBinaryOperands(interpreter, "GEQ");
        interpreter.stack().pushValue(
            RuntimeValue::makeBoolean(compareValues(left, right, "GEQ") >= 0));
        interpreter.advance();
        return;
    }
    case OprCode::GTR: {
        const auto [left, right] = popBinaryOperands(interpreter, "GTR");
        interpreter.stack().pushValue(
            RuntimeValue::makeBoolean(compareValues(left, right, "GTR") > 0));
        interpreter.advance();
        return;
    }
    case OprCode::LEQ: {
        const auto [left, right] = popBinaryOperands(interpreter, "LEQ");
        interpreter.stack().pushValue(
            RuntimeValue::makeBoolean(compareValues(left, right, "LEQ") <= 0));
        interpreter.advance();
        return;
    }
    case OprCode::WRT:
    case OprCode::WRTLN: {
        if (*decoded == OprCode::WRTLN && hint == "writeln empty") {
            interpreter.appendOutput("\n");
            interpreter.advance();
            return;
        }

        const RuntimeValue value =
            popOperand(interpreter,
                       *decoded == OprCode::WRT ? "WRT" : "WRTLN");
        interpreter.appendOutput(value.toString());
        if (*decoded == OprCode::WRTLN) {
            interpreter.appendOutput("\n");
        }
        interpreter.advance();
        return;
    }
    case OprCode::READLN:
        executeReadln(interpreter, instruction);
        return;
    }
}
