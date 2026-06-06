#ifndef CODEGEN_CONTEXT_H
#define CODEGEN_CONTEXT_H

#include "intermediate_code.h"

#include <unordered_map>
#include <vector>

struct RuntimeAddressBinding {
    int level;
    int address;
};

class CodeGenContext {
  public:
    static constexpr int kFrameHeaderSize = 3;

    CodeGenContext();

    void reset();

    int emit(OpCode op, int level, int arg, const std::string &comment = "");
    int emitLiteral(int level, int arg, const std::string &literalText,
                    const std::string &comment = "");
    int emit(const Instruction &instruction);
    void patch(int instructionIndex, int targetLine);

    int nextInstructionIndex() const;
    int globalFrameSize() const;
    int frameSizeForSubprogram(int symbolIndex) const;
    void bindFrameSizeForSubprogram(int symbolIndex, int frameSize);
    void beginGlobalLayout();
    void endGlobalLayout();
    void beginFrameLayout();
    int endFrameLayout();

    bool hasRuntimeAddress(int symbolIndex) const;
    int runtimeAddressOf(int symbolIndex) const;
    int runtimeLevelOf(int symbolIndex) const;
    void bindRuntimeAddress(int symbolIndex, int runtimeLevel, int runtimeAddress);
    int allocateRuntimeAddress(int symbolIndex);

    bool hasSubprogramEntry(int symbolIndex) const;
    int subprogramEntryOf(int symbolIndex) const;
    void bindSubprogramEntry(int symbolIndex, int entryPoint);

    const std::vector<Instruction> &code() const;
    std::vector<Instruction> &code();

  private:
    std::vector<Instruction> code_;
    std::unordered_map<int, RuntimeAddressBinding> runtimeAddressBySymbol_;
    std::unordered_map<int, int> subprogramEntryBySymbol_;
    std::unordered_map<int, int> frameSizeBySubprogram_;
    int nextGlobalAddress_;
    int nextFrameAddress_;
    bool frameLayoutActive_;
};

#endif // CODEGEN_CONTEXT_H
