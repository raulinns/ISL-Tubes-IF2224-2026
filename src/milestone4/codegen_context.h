#ifndef CODEGEN_CONTEXT_H
#define CODEGEN_CONTEXT_H

#include "intermediate_code.h"

#include <unordered_map>
#include <vector>

class CodeGenContext {
  public:
    static constexpr int kFrameHeaderSize = 3;

    CodeGenContext();

    void reset();

    int emit(OpCode op, int level, int arg, const std::string &comment = "");
    int emit(const Instruction &instruction);
    void patch(int instructionIndex, int targetLine);

    int nextInstructionIndex() const;
    int frameSize() const;

    bool hasRuntimeAddress(int symbolIndex) const;
    int runtimeAddressOf(int symbolIndex) const;
    void bindRuntimeAddress(int symbolIndex, int runtimeAddress);
    int allocateRuntimeAddress(int symbolIndex);

    const std::vector<Instruction> &code() const;
    std::vector<Instruction> &code();

  private:
    std::vector<Instruction> code_;
    std::unordered_map<int, int> runtimeAddressBySymbol_;
    int nextRuntimeAddress_;
};

#endif // CODEGEN_CONTEXT_H
