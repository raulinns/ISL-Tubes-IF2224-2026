#ifndef RUNTIME_STACK_H
#define RUNTIME_STACK_H

#include "runtime_value.h"

#include <cstddef>
#include <vector>

struct StackFrame {
    int baseAddress;
    int staticLink;
    int dynamicLink;
    int returnAddress;
    int slotCount;
};

class RuntimeStack {
  public:
    static constexpr std::size_t kDefaultMaxFrameCount = 1000;

    RuntimeStack();
    explicit RuntimeStack(std::size_t maxFrameCount);

    void reset();

    void reserveSlots(int slotCount);
    int allocateSlot();

    bool hasAddress(int address) const;
    const RuntimeValue &readAt(int address) const;
    RuntimeValue &readAt(int address);
    void writeAt(int address, const RuntimeValue &value);

    void pushValue(const RuntimeValue &value);
    RuntimeValue popValue();
    const RuntimeValue &peekValue(std::size_t depth = 0) const;

    void pushFrame(const StackFrame &frame);
    void pushFrame(int baseAddress, int staticLink, int dynamicLink,
                   int returnAddress, int slotCount);
    StackFrame popFrame();

    bool hasFrame() const;
    const StackFrame &currentFrame() const;
    int currentBaseAddress() const;
    std::size_t frameCount() const;
    std::size_t maxFrameCount() const;

    int memorySize() const;
    const std::vector<RuntimeValue> &memory() const;

  private:
    int currentFloor() const;
    void resizeToAtLeast(int slotCount);
    void validateAddress(int address) const;

    std::vector<RuntimeValue> memory_;
    std::vector<StackFrame> frames_;
    std::size_t maxFrameCount_;
    int topLevelFloor_;
};

#endif // RUNTIME_STACK_H
