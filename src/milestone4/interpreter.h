#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "intermediate_code.h"
#include "runtime_stack.h"

#include <cstddef>
#include <string>
#include <vector>

struct InterpreterResult {
    std::string output;
    RuntimeStack stack;
};

class Interpreter {
  public:
    Interpreter();
    explicit Interpreter(std::size_t maxFrameCount);

    void reset();
    void load(std::vector<Instruction> code);

    bool isHalted() const;
    void halt();

    int instructionPointer() const;
    void setInstructionPointer(int value);
    void advance();

    bool hasInstruction(int index) const;
    const Instruction &instructionAt(int index) const;
    const std::vector<Instruction> &code() const;

    RuntimeStack &stack();
    const RuntimeStack &stack() const;

    const std::string &output() const;
    void appendOutput(const std::string &text);
    void clearOutput();

  private:
    std::vector<Instruction> code_;
    RuntimeStack stack_;
    int instructionPointer_;
    bool halted_;
    std::string output_;
};

InterpreterResult runIntermediateCode(
    const std::vector<Instruction> &code,
    std::size_t maxFrameCount = RuntimeStack::kDefaultMaxFrameCount);

void executeInstruction(Interpreter &interpreter, const Instruction &instruction);
void executeOprInstruction(Interpreter &interpreter,
                           const Instruction &instruction);

#endif // INTERPRETER_H
