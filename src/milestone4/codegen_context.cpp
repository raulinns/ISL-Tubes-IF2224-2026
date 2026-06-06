#include "codegen_context.h"

#include <stdexcept>

CodeGenContext::CodeGenContext()
    : nextGlobalAddress_(0), nextFrameAddress_(kFrameHeaderSize),
      frameLayoutActive_(false) {}

void CodeGenContext::reset() {
    code_.clear();
    runtimeAddressBySymbol_.clear();
    subprogramEntryBySymbol_.clear();
    frameSizeBySubprogram_.clear();
    nextGlobalAddress_ = 0;
    nextFrameAddress_ = kFrameHeaderSize;
    frameLayoutActive_ = false;
}

int CodeGenContext::emit(OpCode op, int level, int arg,
                         const std::string &comment) {
    return emit(Instruction(op, level, arg, comment));
}

int CodeGenContext::emitLiteral(int level, int arg,
                                const std::string &literalText,
                                const std::string &comment) {
    return emit(Instruction(OpCode::LIT, level, arg, literalText, comment));
}

int CodeGenContext::emit(const Instruction &instruction) {
    code_.push_back(instruction);
    return static_cast<int>(code_.size()) - 1;
}

void CodeGenContext::patch(int instructionIndex, int targetLine) {
    if (instructionIndex < 0 ||
        instructionIndex >= static_cast<int>(code_.size())) {
        throw std::out_of_range("Instruction index out of range for patch()");
    }
    if (targetLine < 0) {
        throw std::invalid_argument("Patch target line cannot be negative");
    }

    code_[instructionIndex].arg = targetLine;
}

int CodeGenContext::nextInstructionIndex() const {
    return static_cast<int>(code_.size());
}

int CodeGenContext::globalFrameSize() const { return nextGlobalAddress_; }

int CodeGenContext::frameSizeForSubprogram(int symbolIndex) const {
    const auto it = frameSizeBySubprogram_.find(symbolIndex);
    if (it == frameSizeBySubprogram_.end()) {
        throw std::out_of_range("No frame size bound for symbol index " +
                                std::to_string(symbolIndex));
    }
    return it->second;
}

void CodeGenContext::bindFrameSizeForSubprogram(int symbolIndex, int frameSize) {
    if (symbolIndex < 0) {
        throw std::invalid_argument("Symbol index cannot be negative");
    }
    if (frameSize < kFrameHeaderSize) {
        throw std::invalid_argument("Frame size must include header slots");
    }
    frameSizeBySubprogram_[symbolIndex] = frameSize;
}

void CodeGenContext::beginGlobalLayout() {
    frameLayoutActive_ = false;
    nextGlobalAddress_ = 0;
}

void CodeGenContext::endGlobalLayout() {}

void CodeGenContext::beginFrameLayout() {
    frameLayoutActive_ = true;
    nextFrameAddress_ = kFrameHeaderSize;
}

int CodeGenContext::endFrameLayout() {
    frameLayoutActive_ = false;
    return nextFrameAddress_;
}

bool CodeGenContext::hasRuntimeAddress(int symbolIndex) const {
    return runtimeAddressBySymbol_.count(symbolIndex) != 0U;
}

int CodeGenContext::runtimeAddressOf(int symbolIndex) const {
    const auto it = runtimeAddressBySymbol_.find(symbolIndex);
    if (it == runtimeAddressBySymbol_.end()) {
        throw std::out_of_range("No runtime address bound for symbol index " +
                                std::to_string(symbolIndex));
    }
    return it->second.address;
}

int CodeGenContext::runtimeLevelOf(int symbolIndex) const {
    const auto it = runtimeAddressBySymbol_.find(symbolIndex);
    if (it == runtimeAddressBySymbol_.end()) {
        throw std::out_of_range("No runtime address bound for symbol index " +
                                std::to_string(symbolIndex));
    }
    return it->second.level;
}

void CodeGenContext::bindRuntimeAddress(int symbolIndex, int runtimeLevel,
                                        int runtimeAddress) {
    if (symbolIndex < 0) {
        throw std::invalid_argument("Symbol index cannot be negative");
    }
    if (runtimeLevel != 0 && runtimeLevel != 1) {
        throw std::invalid_argument("Runtime level must be 0 (global) or 1 (frame)");
    }

    runtimeAddressBySymbol_[symbolIndex] = {runtimeLevel, runtimeAddress};
    if (runtimeLevel == 0 && runtimeAddress >= nextGlobalAddress_) {
        nextGlobalAddress_ = runtimeAddress + 1;
    } else if (runtimeLevel == 1 && runtimeAddress >= nextFrameAddress_) {
        nextFrameAddress_ = runtimeAddress + 1;
    }
}

int CodeGenContext::allocateRuntimeAddress(int symbolIndex) {
    if (symbolIndex < 0) {
        throw std::invalid_argument("Symbol index cannot be negative");
    }

    const auto it = runtimeAddressBySymbol_.find(symbolIndex);
    if (it != runtimeAddressBySymbol_.end()) {
        return it->second.address;
    }

    const int allocated =
        frameLayoutActive_ ? nextFrameAddress_++ : nextGlobalAddress_++;
    runtimeAddressBySymbol_[symbolIndex] = {frameLayoutActive_ ? 1 : 0, allocated};
    return allocated;
}

bool CodeGenContext::hasSubprogramEntry(int symbolIndex) const {
    return subprogramEntryBySymbol_.count(symbolIndex) != 0U;
}

int CodeGenContext::subprogramEntryOf(int symbolIndex) const {
    const auto it = subprogramEntryBySymbol_.find(symbolIndex);
    if (it == subprogramEntryBySymbol_.end()) {
        throw std::out_of_range("No subprogram entry bound for symbol index " +
                                std::to_string(symbolIndex));
    }
    return it->second;
}

void CodeGenContext::bindSubprogramEntry(int symbolIndex, int entryPoint) {
    if (symbolIndex < 0) {
        throw std::invalid_argument("Symbol index cannot be negative");
    }
    if (entryPoint < 0) {
        throw std::invalid_argument("Subprogram entry point cannot be negative");
    }
    subprogramEntryBySymbol_[symbolIndex] = entryPoint;
}

const std::vector<Instruction> &CodeGenContext::code() const { return code_; }

std::vector<Instruction> &CodeGenContext::code() { return code_; }
