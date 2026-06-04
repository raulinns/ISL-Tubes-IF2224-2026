#include "interpreter.h"

#include "arion_runtime_error.h"

#include <stdexcept>
#include <utility>

namespace {

RuntimeValue literalValueForInstruction(const Instruction &instruction) {
    if (!instruction.literalText.empty()) {
        return RuntimeValue::parseLiteral(instruction.literalText);
    }
    return RuntimeValue::makeInteger(instruction.arg);
}

bool isFalseJumpCondition(const RuntimeValue &value) {
    switch (value.kind()) {
    case RuntimeValueKind::Boolean:
        return !value.asBoolean();
    case RuntimeValueKind::Integer:
        return value.asInteger() == 0;
    case RuntimeValueKind::Real:
    case RuntimeValueKind::Char:
    case RuntimeValueKind::String:
    case RuntimeValueKind::Uninitialized:
        throw RuntimeTypeError("JPC condition must be boolean or integer");
    }

    throw RuntimeTypeError("JPC condition must be boolean or integer");
}

void jumpToInstruction(Interpreter &interpreter, int target) {
    // Jumps may only target existing instructions.
    if (!interpreter.hasInstruction(target)) {
        throw InvalidJumpTargetError("Invalid jump target: " +
                                     std::to_string(target));
    }
    interpreter.setInstructionPointer(target);
}

void validateJumpTargets(const std::vector<Instruction> &code) {
    // Validate all jump targets before the VM starts executing.
    for (std::size_t index = 0; index < code.size(); ++index) {
        const Instruction &instruction = code[index];
        if (instruction.op != OpCode::JMP && instruction.op != OpCode::JPC) {
            continue;
        }
        if (instruction.arg < 0 ||
            instruction.arg >= static_cast<int>(code.size())) {
            throw InvalidJumpTargetError(
                "Invalid jump target at instruction " +
                std::to_string(index) + ": Label not found for target " +
                std::to_string(instruction.arg));
        }
    }
}

[[noreturn]] void unsupportedInterpreterInstruction(const Instruction &instruction,
                                                    const std::string &ownerHint) {
    throw ArionRuntimeError("Milestone 4 placeholder: interpreter instruction " +
                            opcodeToString(instruction.op) + " belongs to " +
                            ownerHint + " integration");
}

} // namespace

Interpreter::Interpreter() : Interpreter(RuntimeStack::kDefaultMaxFrameCount) {}

Interpreter::Interpreter(std::size_t maxFrameCount)
    : stack_(maxFrameCount), instructionPointer_(0), halted_(true) {}

void Interpreter::reset() {
    code_.clear();
    stack_.reset();
    instructionPointer_ = 0;
    halted_ = true;
    output_.clear();
}

void Interpreter::load(std::vector<Instruction> code) {
    reset();
    validateJumpTargets(code);
    code_ = std::move(code);
    halted_ = code_.empty();
}

bool Interpreter::isHalted() const { return halted_; }

void Interpreter::halt() { halted_ = true; }

int Interpreter::instructionPointer() const { return instructionPointer_; }

void Interpreter::setInstructionPointer(int value) {
    if (value < 0 || value > static_cast<int>(code_.size())) {
        throw InvalidJumpTargetError("Invalid instruction pointer target: " +
                                     std::to_string(value));
    }
    instructionPointer_ = value;
    if (instructionPointer_ == static_cast<int>(code_.size())) {
        halted_ = true;
    }
}

void Interpreter::advance() { setInstructionPointer(instructionPointer_ + 1); }

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

InterpreterResult runIntermediateCode(const std::vector<Instruction> &code,
                                      std::size_t maxFrameCount) {
    Interpreter interpreter(maxFrameCount);
    interpreter.load(code);

    while (!interpreter.isHalted()) {
        const Instruction instruction =
            interpreter.instructionAt(interpreter.instructionPointer());
        executeInstruction(interpreter, instruction);
    }

    return {interpreter.output(), interpreter.stack()};
}

void executeInstruction(Interpreter &interpreter,
                        const Instruction &instruction) {
    switch (instruction.op) {
    case OpCode::INT:
        interpreter.stack().reserveSlots(instruction.arg);
        interpreter.advance();
        return;
    case OpCode::LIT:
        interpreter.stack().pushValue(literalValueForInstruction(instruction));
        interpreter.advance();
        return;
    case OpCode::LOD:
        interpreter.stack().pushValue(interpreter.stack().readAt(instruction.arg));
        interpreter.advance();
        return;
    case OpCode::STO: {
        RuntimeValue value = interpreter.stack().popValue();
        interpreter.stack().writeAt(instruction.arg, value);
        interpreter.advance();
        return;
    }
    case OpCode::RET:
        if (interpreter.stack().hasFrame()) {
            unsupportedInterpreterInstruction(instruction, "Endra");
        }
        interpreter.halt();
        return;
    case OpCode::CAL:
        unsupportedInterpreterInstruction(instruction, "Endra");
    case OpCode::JMP:
        jumpToInstruction(interpreter, instruction.arg);
        return;
    case OpCode::JPC:
        if (isFalseJumpCondition(interpreter.stack().popValue())) {
            jumpToInstruction(interpreter, instruction.arg);
        } else {
            interpreter.advance();
        }
        return;
    case OpCode::OPR:
        executeOprInstruction(interpreter, instruction);
        return;
    }

    throw ArionRuntimeError("Unknown interpreter opcode");
}

