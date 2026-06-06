#include "codegen_context.h"

#include <stdexcept>

void CodeGenContext::reset() {
    code_.clear();
    subprogramEntryBySymbol_.clear();
    callPatchesBySymbol_.clear();
    currentLexicalLevel_ = 0;
}

int CodeGenContext::emit(OpCode op, int level, int arg,
                         const std::string &comment) {
    code_.emplace_back(op, level, arg, comment);
    return static_cast<int>(code_.size()) - 1;
}

int CodeGenContext::emit(OpCode op, int level, int arg,
                         std::vector<int> extraArgs,
                         const std::string &comment) {
    code_.emplace_back(op, level, arg, std::move(extraArgs), comment);
    return static_cast<int>(code_.size()) - 1;
}

int CodeGenContext::emitLiteral(int level, int arg,
                                const std::string &literalText,
                                const std::string &comment) {
    code_.emplace_back(OpCode::LIT, level, arg, literalText, comment);
    return static_cast<int>(code_.size()) - 1;
}

void CodeGenContext::patch(int instructionIndex, int targetLine) {
    if (instructionIndex < 0 ||
        instructionIndex >= static_cast<int>(code_.size()) ||
        targetLine < 0) {
        throw std::out_of_range("Invalid intermediate-code patch");
    }
    code_[static_cast<std::size_t>(instructionIndex)].arg = targetLine;
}

int CodeGenContext::nextInstructionIndex() const {
    return static_cast<int>(code_.size());
}

int CodeGenContext::currentLexicalLevel() const {
    return currentLexicalLevel_;
}

void CodeGenContext::setCurrentLexicalLevel(int level) {
    if (level < 0) {
        throw std::invalid_argument("Lexical level cannot be negative");
    }
    currentLexicalLevel_ = level;
}

bool CodeGenContext::hasSubprogramEntry(int symbolIndex) const {
    return subprogramEntryBySymbol_.count(symbolIndex) != 0;
}

int CodeGenContext::subprogramEntryOf(int symbolIndex) const {
    const auto found = subprogramEntryBySymbol_.find(symbolIndex);
    if (found == subprogramEntryBySymbol_.end()) {
        throw std::out_of_range("Subprogram entry is not bound");
    }
    return found->second;
}

void CodeGenContext::bindSubprogramEntry(int symbolIndex, int entryPoint) {
    subprogramEntryBySymbol_[symbolIndex] = entryPoint;
    const auto patches = callPatchesBySymbol_.find(symbolIndex);
    if (patches != callPatchesBySymbol_.end()) {
        for (const int instruction : patches->second) {
            patch(instruction, entryPoint);
        }
        callPatchesBySymbol_.erase(patches);
    }
}

void CodeGenContext::addCallPatch(int symbolIndex, int instructionIndex) {
    if (hasSubprogramEntry(symbolIndex)) {
        patch(instructionIndex, subprogramEntryOf(symbolIndex));
    } else {
        callPatchesBySymbol_[symbolIndex].push_back(instructionIndex);
    }
}

void CodeGenContext::validateCallPatches() const {
    if (!callPatchesBySymbol_.empty()) {
        throw std::runtime_error("Unresolved procedure/function call target");
    }
}

const std::vector<Instruction> &CodeGenContext::code() const { return code_; }
std::vector<Instruction> &CodeGenContext::code() { return code_; }
