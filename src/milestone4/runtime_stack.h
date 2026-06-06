#ifndef RUNTIME_STACK_H
#define RUNTIME_STACK_H

#include "runtime_value.h"

#include <cstddef>
#include <vector>

struct StackFrame {
    int lexicalLevel;
    int staticLink;
    int dynamicLink;
    int returnAddress;
    int slotCount;
    std::size_t operandBase;
    std::vector<RuntimeValue> slots;
};

class RuntimeStack {
  public:
    static constexpr std::size_t kDefaultMaxFrameCount = 1000;
    static constexpr int kFrameHeaderSize = 3;

    RuntimeStack();
    explicit RuntimeStack(std::size_t maxFrameCount);

    void reset();
    void reserveSlots(int slotCount);

    RuntimeAddress resolveAddress(int levelDifference, int offset) const;
    const RuntimeValue &readAt(int levelDifference, int offset) const;
    void writeAt(int levelDifference, int offset, const RuntimeValue &value);
    const RuntimeValue &readAddress(const RuntimeAddress &address) const;
    void writeAddress(const RuntimeAddress &address, const RuntimeValue &value);
    std::vector<RuntimeValue> readBlock(const RuntimeAddress &address,
                                        int size) const;
    void writeBlock(const RuntimeAddress &address,
                    const std::vector<RuntimeValue> &values);

    void pushValue(const RuntimeValue &value);
    RuntimeValue popValue();
    const RuntimeValue &peekValue(std::size_t depth = 0) const;
    std::size_t operandDepth() const;

    void pushCallFrame(int lexicalDifference, int returnAddress,
                       int argumentSlotCount);
    StackFrame popFrame();

    bool hasFrame() const;
    bool hasCallerFrame() const;
    const StackFrame &currentFrame() const;
    std::size_t frameCount() const;
    std::size_t maxFrameCount() const;
    std::size_t currentOperandBase() const;
    int currentReturnAddress() const;

  private:
    int frameIndexAtDifference(int levelDifference) const;
    void validateAddress(const RuntimeAddress &address) const;

    std::vector<StackFrame> frames_;
    std::vector<RuntimeValue> operands_;
    std::size_t maxFrameCount_;
};

#endif // RUNTIME_STACK_H
