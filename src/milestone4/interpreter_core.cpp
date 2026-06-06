#include "interpreter.h"

#include "arion_runtime_error.h"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

RuntimeValue literalValueForInstruction(const Instruction &instruction) {
    if (!instruction.literalText.empty()) {
        return RuntimeValue::parseLiteral(instruction.literalText);
    }
    return RuntimeValue::makeInteger(instruction.arg);
}

bool isFalseCondition(const RuntimeValue &value) {
    if (value.kind() == RuntimeValueKind::Boolean) {
        return !value.asBoolean();
    }
    if (value.kind() == RuntimeValueKind::Integer) {
        return value.asInteger() == 0;
    }
    throw RuntimeTypeError("JPC condition must be boolean");
}

int extraAt(const Instruction &instruction, std::size_t index,
            int defaultValue = 0) {
    return index < instruction.extraArgs.size()
               ? instruction.extraArgs[index]
               : defaultValue;
}

void validateTargets(const std::vector<Instruction> &code) {
    for (std::size_t i = 0; i < code.size(); ++i) {
        const Instruction &instruction = code[i];
        if (instruction.op != OpCode::JMP &&
            instruction.op != OpCode::JPC &&
            instruction.op != OpCode::CAL) {
            continue;
        }
        if (instruction.arg < 0 ||
            instruction.arg >= static_cast<int>(code.size())) {
            throw InvalidJumpTargetError(
                "ip " + std::to_string(i) + ": invalid jump/call target " +
                std::to_string(instruction.arg));
        }
        if (instruction.op == OpCode::CAL) {
            const OpCode targetOp =
                code[static_cast<std::size_t>(instruction.arg)].op;
            if (targetOp != OpCode::INT && targetOp != OpCode::JMP) {
                throw InvalidJumpTargetError(
                    "ip " + std::to_string(i) +
                    ": call target is not a subprogram entry");
            }
        }
    }
}

} // namespace

Interpreter::Interpreter() : Interpreter(RuntimeStack::kDefaultMaxFrameCount) {}

Interpreter::Interpreter(std::size_t maxFrameCount)
    : stack_(maxFrameCount), instructionPointer_(0), halted_(true),
      input_(&std::cin) {}

void Interpreter::reset() {
    code_.clear();
    stack_.reset();
    instructionPointer_ = 0;
    halted_ = true;
    output_.clear();
}

void Interpreter::load(std::vector<Instruction> code) {
    reset();
    validateTargets(code);
    code_ = std::move(code);
    halted_ = code_.empty();
}

bool Interpreter::isHalted() const { return halted_; }
void Interpreter::halt() { halted_ = true; }
int Interpreter::instructionPointer() const { return instructionPointer_; }

void Interpreter::setInstructionPointer(int value) {
    if (value < 0 || value >= static_cast<int>(code_.size())) {
        throw InvalidJumpTargetError("Invalid instruction pointer target: " +
                                     std::to_string(value));
    }
    instructionPointer_ = value;
}

void Interpreter::advance() {
    if (instructionPointer_ + 1 >= static_cast<int>(code_.size())) {
        throw StackCorruptionError(
            "Program reached end of code without RET");
    }
    ++instructionPointer_;
}

bool Interpreter::hasInstruction(int index) const {
    return index >= 0 && index < static_cast<int>(code_.size());
}

const Instruction &Interpreter::instructionAt(int index) const {
    if (!hasInstruction(index)) {
        throw InvalidJumpTargetError("No instruction at index " +
                                     std::to_string(index));
    }
    return code_[static_cast<std::size_t>(index)];
}

const std::vector<Instruction> &Interpreter::code() const { return code_; }
RuntimeStack &Interpreter::stack() { return stack_; }
const RuntimeStack &Interpreter::stack() const { return stack_; }
const std::string &Interpreter::output() const { return output_; }
void Interpreter::appendOutput(const std::string &text) { output_ += text; }
void Interpreter::clearOutput() { output_.clear(); }
std::istream &Interpreter::input() { return *input_; }
void Interpreter::setInput(std::istream &input) { input_ = &input; }

InterpreterResult runIntermediateCode(const std::vector<Instruction> &code,
                                      std::size_t maxFrameCount) {
    return runIntermediateCode(code, std::cin, maxFrameCount);
}

InterpreterResult runIntermediateCode(const std::vector<Instruction> &code,
                                      std::istream &input,
                                      std::size_t maxFrameCount) {
    Interpreter interpreter(maxFrameCount);
    interpreter.setInput(input);
    interpreter.load(code);
    while (!interpreter.isHalted()) {
        const int ip = interpreter.instructionPointer();
        try {
            executeInstruction(interpreter, interpreter.instructionAt(ip));
        } catch (const BoundsError &error) {
            throw BoundsError("ip " + std::to_string(ip) + ": " + error.what());
        } catch (const NumericOverflowError &error) {
            throw NumericOverflowError("ip " + std::to_string(ip) + ": " +
                                       error.what());
        } catch (const NumericUnderflowError &error) {
            throw NumericUnderflowError("ip " + std::to_string(ip) + ": " +
                                        error.what());
        } catch (const InvalidMemoryError &error) {
            throw InvalidMemoryError("ip " + std::to_string(ip) + ": " +
                                     error.what());
        } catch (const StackCorruptionError &error) {
            throw StackCorruptionError("ip " + std::to_string(ip) + ": " +
                                       error.what());
        } catch (const StackOverflowError &error) {
            throw StackOverflowError("ip " + std::to_string(ip) + ": " +
                                     error.what());
        } catch (const StackUnderflowError &error) {
            throw StackUnderflowError("ip " + std::to_string(ip) + ": " +
                                      error.what());
        } catch (const InvalidJumpTargetError &error) {
            throw InvalidJumpTargetError("ip " + std::to_string(ip) + ": " +
                                         error.what());
        } catch (const DivisionByZeroError &error) {
            throw DivisionByZeroError("ip " + std::to_string(ip) + ": " +
                                      error.what());
        } catch (const RuntimeTypeError &error) {
            throw RuntimeTypeError("ip " + std::to_string(ip) + ": " +
                                   error.what());
        } catch (const ArionRuntimeError &error) {
            throw ArionRuntimeError("ip " + std::to_string(ip) + ": " +
                                    error.what());
        } catch (const std::exception &error) {
            throw ArionRuntimeError("ip " + std::to_string(ip) + ": " +
                                    error.what());
        }
    }
    if (interpreter.stack().frameCount() != 1 ||
        interpreter.stack().operandDepth() != 0) {
        throw StackCorruptionError(
            "ip halt: unbalanced frames or operands after program return");
    }
    return {interpreter.output(), interpreter.stack()};
}

void executeInstruction(Interpreter &interpreter,
                        const Instruction &instruction) {
    RuntimeStack &stack = interpreter.stack();
    switch (instruction.op) {
    case OpCode::INT:
        stack.reserveSlots(instruction.arg);
        interpreter.advance();
        return;
    case OpCode::LIT:
        stack.pushValue(literalValueForInstruction(instruction));
        interpreter.advance();
        return;
    case OpCode::LOD:
        stack.pushValue(stack.readAt(instruction.level, instruction.arg));
        interpreter.advance();
        return;
    case OpCode::STO:
        stack.writeAt(instruction.level, instruction.arg, stack.popValue());
        interpreter.advance();
        return;
    case OpCode::LDA: {
        const RuntimeAddress address =
            stack.resolveAddress(instruction.level, instruction.arg);
        stack.pushValue(
            RuntimeValue::makeAddress(address.frameIndex, address.offset));
        interpreter.advance();
        return;
    }
    case OpCode::LDI: {
        const RuntimeAddress address = stack.popValue().asAddress();
        stack.pushValue(stack.readAddress(address));
        interpreter.advance();
        return;
    }
    case OpCode::STI: {
        const RuntimeAddress address = stack.popValue().asAddress();
        const RuntimeValue value = stack.popValue();
        stack.writeAddress(address, value);
        interpreter.advance();
        return;
    }
    case OpCode::BLD: {
        const int size = instruction.arg;
        const RuntimeAddress address = stack.popValue().asAddress();
        for (const RuntimeValue &value : stack.readBlock(address, size)) {
            stack.pushValue(value);
        }
        interpreter.advance();
        return;
    }
    case OpCode::BST: {
        const int size = instruction.arg;
        if (size < 0) {
            throw InvalidMemoryError("Negative BST block size");
        }
        const RuntimeAddress address = stack.popValue().asAddress();
        std::vector<RuntimeValue> values(static_cast<std::size_t>(size));
        for (int i = size - 1; i >= 0; --i) {
            values[static_cast<std::size_t>(i)] = stack.popValue();
        }
        stack.writeBlock(address, values);
        interpreter.advance();
        return;
    }
    case OpCode::ADI: {
        RuntimeAddress address = stack.popValue().asAddress();
        address.offset += instruction.arg;
        stack.readBlock(address, 0);
        stack.pushValue(
            RuntimeValue::makeAddress(address.frameIndex, address.offset));
        interpreter.advance();
        return;
    }
    case OpCode::IXA: {
        const int lower = instruction.arg;
        const int upper = extraAt(instruction, 0);
        const int elementSize = extraAt(instruction, 1, 1);
        const int index = stack.popValue().asInteger();
        RuntimeAddress address = stack.popValue().asAddress();
        if (index < lower || index > upper) {
            throw BoundsError("array index " + std::to_string(index) +
                              " outside " + std::to_string(lower) + ".." +
                              std::to_string(upper));
        }
        address.offset += (index - lower) * elementSize;
        stack.pushValue(
            RuntimeValue::makeAddress(address.frameIndex, address.offset));
        interpreter.advance();
        return;
    }
    case OpCode::CAL:
        stack.pushCallFrame(instruction.level,
                            interpreter.instructionPointer() + 1,
                            extraAt(instruction, 0));
        interpreter.setInstructionPointer(instruction.arg);
        return;
    case OpCode::RET: {
        const int resultSize = extraAt(instruction, 0);
        if (!stack.hasCallerFrame()) {
            if (resultSize != 0 || stack.operandDepth() != 0) {
                throw StackCorruptionError("Invalid global RET state");
            }
            interpreter.halt();
            return;
        }
        if (stack.operandDepth() != stack.currentOperandBase()) {
            throw StackCorruptionError(
                "Operand depth does not match CAL/RET contract");
        }
        std::vector<RuntimeValue> result;
        if (resultSize > 0) {
            result = stack.readBlock(
                stack.resolveAddress(0, instruction.arg), resultSize);
        }
        const StackFrame frame = stack.popFrame();
        for (const RuntimeValue &value : result) {
            stack.pushValue(value);
        }
        interpreter.setInstructionPointer(frame.returnAddress);
        return;
    }
    case OpCode::JMP:
        interpreter.setInstructionPointer(instruction.arg);
        return;
    case OpCode::JPC:
        if (isFalseCondition(stack.popValue())) {
            interpreter.setInstructionPointer(instruction.arg);
        } else {
            interpreter.advance();
        }
        return;
    case OpCode::OPR:
        executeOprInstruction(interpreter, instruction);
        return;
    }
}
