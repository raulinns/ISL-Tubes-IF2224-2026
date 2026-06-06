#ifndef CODEGEN_CONTEXT_H
#define CODEGEN_CONTEXT_H

#include "intermediate_code.h"

#include <unordered_map>
#include <vector>

class CodeGenContext {
  public:
    static constexpr int kFrameHeaderSize = 3;

    void reset();
    int emit(OpCode op, int level, int arg, const std::string &comment = "");
    int emit(OpCode op, int level, int arg, std::vector<int> extraArgs,
             const std::string &comment = "");
    int emitLiteral(int level, int arg, const std::string &literalText,
                    const std::string &comment = "");
    void patch(int instructionIndex, int targetLine);

    int nextInstructionIndex() const;
    int currentLexicalLevel() const;
    void setCurrentLexicalLevel(int level);

    bool hasSubprogramEntry(int symbolIndex) const;
    int subprogramEntryOf(int symbolIndex) const;
    void bindSubprogramEntry(int symbolIndex, int entryPoint);
    void addCallPatch(int symbolIndex, int instructionIndex);
    void validateCallPatches() const;

    const std::vector<Instruction> &code() const;
    std::vector<Instruction> &code();

  private:
    std::vector<Instruction> code_;
    std::unordered_map<int, int> subprogramEntryBySymbol_;
    std::unordered_map<int, std::vector<int>> callPatchesBySymbol_;
    int currentLexicalLevel_ = 0;
};

#endif // CODEGEN_CONTEXT_H
