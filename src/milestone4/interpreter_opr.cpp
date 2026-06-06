#include "interpreter.h"

#include "arion_runtime_error.h"

#include <cmath>
#include <cstdint>
#include <istream>
#include <limits>
#include <string>
#include <utility>

namespace {

RuntimeValue popOperand(Interpreter &interpreter, const std::string &name) {
    try {
        return interpreter.stack().popValue();
    } catch (const StackUnderflowError &) {
        throw StackUnderflowError(name + " requires more operands");
    }
}

std::pair<RuntimeValue, RuntimeValue>
popBinary(Interpreter &interpreter, const std::string &name) {
    RuntimeValue right = popOperand(interpreter, name);
    RuntimeValue left = popOperand(interpreter, name);
    return {std::move(left), std::move(right)};
}

void requireNumeric(const RuntimeValue &left, const RuntimeValue &right,
                    const std::string &name) {
    if (!left.isNumeric() || !right.isNumeric()) {
        throw RuntimeTypeError(name + " requires numeric operands");
    }
}

int requireInteger(const RuntimeValue &value, const std::string &name) {
    if (value.kind() != RuntimeValueKind::Integer) {
        throw RuntimeTypeError(name + " requires integer operands");
    }
    return value.asInteger();
}

bool requireBoolean(const RuntimeValue &value, const std::string &name) {
    if (value.kind() != RuntimeValueKind::Boolean) {
        throw RuntimeTypeError(name + " requires boolean operands");
    }
    return value.asBoolean();
}

int checkedInteger(std::int64_t value, const std::string &name) {
    if (value > std::numeric_limits<int>::max()) {
        throw NumericOverflowError(name + " integer overflow");
    }
    if (value < std::numeric_limits<int>::min()) {
        throw NumericUnderflowError(name + " integer underflow");
    }
    return static_cast<int>(value);
}

double checkedReal(double value, const std::string &name,
                   bool unexpectedZero = false) {
    if (!std::isfinite(value)) {
        throw NumericOverflowError(name + " real overflow");
    }
    if (std::fpclassify(value) == FP_SUBNORMAL ||
        (unexpectedZero && value == 0.0)) {
        throw NumericUnderflowError(name + " real underflow");
    }
    return value;
}

RuntimeValue add(const RuntimeValue &left, const RuntimeValue &right) {
    requireNumeric(left, right, "ADD");
    if (left.kind() == RuntimeValueKind::Integer &&
        right.kind() == RuntimeValueKind::Integer) {
        return RuntimeValue::makeInteger(checkedInteger(
            static_cast<std::int64_t>(left.asInteger()) + right.asInteger(),
            "ADD"));
    }
    return RuntimeValue::makeReal(
        checkedReal(left.asReal() + right.asReal(), "ADD"));
}

RuntimeValue subtract(const RuntimeValue &left, const RuntimeValue &right) {
    requireNumeric(left, right, "SUB");
    if (left.kind() == RuntimeValueKind::Integer &&
        right.kind() == RuntimeValueKind::Integer) {
        return RuntimeValue::makeInteger(checkedInteger(
            static_cast<std::int64_t>(left.asInteger()) - right.asInteger(),
            "SUB"));
    }
    return RuntimeValue::makeReal(
        checkedReal(left.asReal() - right.asReal(), "SUB"));
}

RuntimeValue multiply(const RuntimeValue &left, const RuntimeValue &right) {
    requireNumeric(left, right, "MUL");
    if (left.kind() == RuntimeValueKind::Integer &&
        right.kind() == RuntimeValueKind::Integer) {
        return RuntimeValue::makeInteger(checkedInteger(
            static_cast<std::int64_t>(left.asInteger()) * right.asInteger(),
            "MUL"));
    }
    const double lhs = left.asReal();
    const double rhs = right.asReal();
    return RuntimeValue::makeReal(
        checkedReal(lhs * rhs, "MUL", lhs != 0.0 && rhs != 0.0));
}

RuntimeValue realDivide(const RuntimeValue &left, const RuntimeValue &right) {
    requireNumeric(left, right, "DIV");
    const double divisor = right.asReal();
    if (divisor == 0.0) {
        throw DivisionByZeroError("Division by zero");
    }
    const double dividend = left.asReal();
    return RuntimeValue::makeReal(checkedReal(
        dividend / divisor, "DIV", dividend != 0.0));
}

RuntimeValue integerDivide(const RuntimeValue &left,
                           const RuntimeValue &right) {
    const int dividend = requireInteger(left, "IDIV");
    const int divisor = requireInteger(right, "IDIV");
    if (divisor == 0) {
        throw DivisionByZeroError("Integer division by zero");
    }
    if (dividend == std::numeric_limits<int>::min() && divisor == -1) {
        throw NumericOverflowError("IDIV integer overflow");
    }
    return RuntimeValue::makeInteger(dividend / divisor);
}

RuntimeValue modulo(const RuntimeValue &left, const RuntimeValue &right) {
    const int dividend = requireInteger(left, "MOD");
    const int divisor = requireInteger(right, "MOD");
    if (divisor == 0) {
        throw DivisionByZeroError("Modulo by zero");
    }
    if (dividend == std::numeric_limits<int>::min() && divisor == -1) {
        return RuntimeValue::makeInteger(0);
    }
    return RuntimeValue::makeInteger(dividend % divisor);
}

int compare(const RuntimeValue &left, const RuntimeValue &right,
            const std::string &name) {
    if (left.isNumeric() && right.isNumeric()) {
        const double lhs = left.asReal();
        const double rhs = right.asReal();
        return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
    }
    if (left.kind() != right.kind()) {
        throw RuntimeTypeError(name + " requires compatible operands");
    }
    switch (left.kind()) {
    case RuntimeValueKind::Boolean:
        return left.asBoolean() == right.asBoolean()
                   ? 0
                   : (left.asBoolean() ? 1 : -1);
    case RuntimeValueKind::Char:
        return left.asChar() < right.asChar()
                   ? -1
                   : (left.asChar() > right.asChar() ? 1 : 0);
    case RuntimeValueKind::String:
        return left.asString() < right.asString()
                   ? -1
                   : (left.asString() > right.asString() ? 1 : 0);
    default:
        throw RuntimeTypeError(name + " does not accept this operand type");
    }
}

RuntimeValue readValue(Interpreter &interpreter, int typeCode) {
    std::string token;
    if (!(interpreter.input() >> token)) {
        throw ArionRuntimeError("readln expected an input value");
    }
    try {
        switch (typeCode) {
        case 1:
            return RuntimeValue::parseLiteral(token, "integer");
        case 2:
            return RuntimeValue::parseLiteral(token, "real");
        case 3:
            return RuntimeValue::parseLiteral(token, "boolean");
        case 4:
            if (token.size() != 1) {
                throw std::invalid_argument("char input must have length one");
            }
            return RuntimeValue::makeChar(token[0]);
        case 5:
            return RuntimeValue::makeString(token);
        default:
            throw RuntimeTypeError("READLN has unsupported type code");
        }
    } catch (const ArionRuntimeError &) {
        throw;
    } catch (const std::exception &error) {
        throw RuntimeTypeError("readln input '" + token +
                               "' has invalid type: " + error.what());
    }
}

} // namespace

void executeOprInstruction(Interpreter &interpreter,
                           const Instruction &instruction) {
    const std::optional<OprCode> decoded = decodeOprCode(instruction.arg);
    if (!decoded) {
        throw ArionRuntimeError("Unknown OPR code " +
                                std::to_string(instruction.arg));
    }
    RuntimeStack &stack = interpreter.stack();
    switch (*decoded) {
    case OprCode::NEG: {
        const RuntimeValue value = popOperand(interpreter, "NEG");
        if (value.kind() == RuntimeValueKind::Integer) {
            stack.pushValue(RuntimeValue::makeInteger(checkedInteger(
                -static_cast<std::int64_t>(value.asInteger()), "NEG")));
        } else if (value.kind() == RuntimeValueKind::Real) {
            stack.pushValue(
                RuntimeValue::makeReal(checkedReal(-value.asReal(), "NEG")));
        } else {
            throw RuntimeTypeError("NEG requires a numeric operand");
        }
        break;
    }
    case OprCode::ADD: {
        const auto [left, right] = popBinary(interpreter, "ADD");
        stack.pushValue(add(left, right));
        break;
    }
    case OprCode::SUB: {
        const auto [left, right] = popBinary(interpreter, "SUB");
        stack.pushValue(subtract(left, right));
        break;
    }
    case OprCode::MUL: {
        const auto [left, right] = popBinary(interpreter, "MUL");
        stack.pushValue(multiply(left, right));
        break;
    }
    case OprCode::DIV: {
        const auto [left, right] = popBinary(interpreter, "DIV");
        stack.pushValue(realDivide(left, right));
        break;
    }
    case OprCode::IDIV: {
        const auto [left, right] = popBinary(interpreter, "IDIV");
        stack.pushValue(integerDivide(left, right));
        break;
    }
    case OprCode::MOD: {
        const auto [left, right] = popBinary(interpreter, "MOD");
        stack.pushValue(modulo(left, right));
        break;
    }
    case OprCode::AND: {
        const auto [left, right] = popBinary(interpreter, "AND");
        stack.pushValue(RuntimeValue::makeBoolean(
            requireBoolean(left, "AND") && requireBoolean(right, "AND")));
        break;
    }
    case OprCode::OR: {
        const auto [left, right] = popBinary(interpreter, "OR");
        stack.pushValue(RuntimeValue::makeBoolean(
            requireBoolean(left, "OR") || requireBoolean(right, "OR")));
        break;
    }
    case OprCode::NOT:
        stack.pushValue(RuntimeValue::makeBoolean(
            !requireBoolean(popOperand(interpreter, "NOT"), "NOT")));
        break;
    case OprCode::EQL:
    case OprCode::NEQ:
    case OprCode::LSS:
    case OprCode::GEQ:
    case OprCode::GTR:
    case OprCode::LEQ: {
        const auto [left, right] =
            popBinary(interpreter, oprCodeToString(*decoded));
        const int result = compare(left, right, oprCodeToString(*decoded));
        bool value = false;
        if (*decoded == OprCode::EQL) value = result == 0;
        if (*decoded == OprCode::NEQ) value = result != 0;
        if (*decoded == OprCode::LSS) value = result < 0;
        if (*decoded == OprCode::GEQ) value = result >= 0;
        if (*decoded == OprCode::GTR) value = result > 0;
        if (*decoded == OprCode::LEQ) value = result <= 0;
        stack.pushValue(RuntimeValue::makeBoolean(value));
        break;
    }
    case OprCode::WRT:
        interpreter.appendOutput(popOperand(interpreter, "WRT").toString());
        break;
    case OprCode::WRTLN:
        if (instruction.extraArgs.empty() || instruction.extraArgs[0] == 0) {
            interpreter.appendOutput(
                popOperand(interpreter, "WRTLN").toString());
        }
        interpreter.appendOutput("\n");
        break;
    case OprCode::READLN: {
        const RuntimeAddress address =
            popOperand(interpreter, "READLN").asAddress();
        const int typeCode =
            instruction.extraArgs.empty() ? 0 : instruction.extraArgs[0];
        stack.writeAddress(address, readValue(interpreter, typeCode));
        break;
    }
    }
    interpreter.advance();
}
