#include "runtime_stack.h"

#include "arion_runtime_error.h"

#include <stdexcept>
#include <string>

RuntimeStack::RuntimeStack()
    : RuntimeStack(RuntimeStack::kDefaultMaxFrameCount) {}

RuntimeStack::RuntimeStack(std::size_t maxFrameCount)
    : maxFrameCount_(maxFrameCount) {
    if (maxFrameCount == 0) {
        throw std::invalid_argument("Maximum frame count must be positive");
    }
}

void RuntimeStack::reset() {
    frames_.clear();
    operands_.clear();
}

void RuntimeStack::reserveSlots(int slotCount) {
    if (slotCount < kFrameHeaderSize) {
        throw StackCorruptionError("INT frame size is smaller than frame header");
    }
    if (frames_.empty()) {
        frames_.push_back(
            {0, -1, -1, -1, slotCount, 0,
             std::vector<RuntimeValue>(static_cast<std::size_t>(slotCount))});
        return;
    }
    StackFrame &frame = frames_.back();
    if (frame.slotCount != 0) {
        throw StackCorruptionError("Activation frame executes INT more than once");
    }
    frame.slotCount = slotCount;
    frame.slots.resize(static_cast<std::size_t>(slotCount));
}

int RuntimeStack::frameIndexAtDifference(int levelDifference) const {
    if (frames_.empty()) {
        throw StackUnderflowError("No activation frame is active");
    }
    if (levelDifference < 0) {
        throw InvalidMemoryError("Negative lexical-level difference");
    }
    int index = static_cast<int>(frames_.size()) - 1;
    for (int i = 0; i < levelDifference; ++i) {
        index = frames_[static_cast<std::size_t>(index)].staticLink;
        if (index < 0 || index >= static_cast<int>(frames_.size())) {
            throw StackCorruptionError("Broken static link");
        }
    }
    return index;
}

RuntimeAddress RuntimeStack::resolveAddress(int levelDifference,
                                            int offset) const {
    const RuntimeAddress address{frameIndexAtDifference(levelDifference), offset};
    validateAddress(address);
    return address;
}

const RuntimeValue &RuntimeStack::readAt(int levelDifference, int offset) const {
    return readAddress(resolveAddress(levelDifference, offset));
}

void RuntimeStack::writeAt(int levelDifference, int offset,
                           const RuntimeValue &value) {
    writeAddress(resolveAddress(levelDifference, offset), value);
}

const RuntimeValue &
RuntimeStack::readAddress(const RuntimeAddress &address) const {
    validateAddress(address);
    const RuntimeValue &value =
        frames_[static_cast<std::size_t>(address.frameIndex)]
            .slots[static_cast<std::size_t>(address.offset)];
    if (!value.isInitialized()) {
        throw InvalidMemoryError("Read from uninitialized runtime slot");
    }
    return value;
}

void RuntimeStack::writeAddress(const RuntimeAddress &address,
                                const RuntimeValue &value) {
    validateAddress(address);
    frames_[static_cast<std::size_t>(address.frameIndex)]
        .slots[static_cast<std::size_t>(address.offset)] = value;
}

std::vector<RuntimeValue>
RuntimeStack::readBlock(const RuntimeAddress &address, int size) const {
    if (size < 0) {
        throw InvalidMemoryError("Negative block size");
    }
    std::vector<RuntimeValue> values;
    values.reserve(static_cast<std::size_t>(size));
    for (int i = 0; i < size; ++i) {
        values.push_back(
            readAddress({address.frameIndex, address.offset + i}));
    }
    return values;
}

void RuntimeStack::writeBlock(const RuntimeAddress &address,
                              const std::vector<RuntimeValue> &values) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        writeAddress({address.frameIndex,
                      address.offset + static_cast<int>(i)},
                     values[i]);
    }
}

void RuntimeStack::pushValue(const RuntimeValue &value) {
    operands_.push_back(value);
}

RuntimeValue RuntimeStack::popValue() {
    if (operands_.empty()) {
        throw StackUnderflowError("Operand stack underflow");
    }
    RuntimeValue value = operands_.back();
    operands_.pop_back();
    return value;
}

const RuntimeValue &RuntimeStack::peekValue(std::size_t depth) const {
    if (depth >= operands_.size()) {
        throw StackUnderflowError("Operand stack underflow");
    }
    return operands_[operands_.size() - depth - 1];
}

std::size_t RuntimeStack::operandDepth() const { return operands_.size(); }

void RuntimeStack::pushCallFrame(int lexicalDifference, int returnAddress,
                                 int argumentSlotCount) {
    if (frames_.size() >= maxFrameCount_) {
        throw StackOverflowError("Activation frame limit exceeded");
    }
    if (argumentSlotCount < 0 ||
        static_cast<std::size_t>(argumentSlotCount) > operands_.size()) {
        throw StackCorruptionError("CAL argument-slot count exceeds operand stack");
    }
    const int staticParent = frameIndexAtDifference(lexicalDifference);
    const int dynamicParent = static_cast<int>(frames_.size()) - 1;
    const int lexicalLevel =
        frames_[static_cast<std::size_t>(staticParent)].lexicalLevel + 1;
    frames_.push_back(
        {lexicalLevel, staticParent, dynamicParent, returnAddress, 0,
         operands_.size() - static_cast<std::size_t>(argumentSlotCount), {}});
}

StackFrame RuntimeStack::popFrame() {
    if (frames_.size() <= 1) {
        throw StackUnderflowError("Cannot pop the global activation frame");
    }
    const StackFrame frame = frames_.back();
    if (frame.dynamicLink != static_cast<int>(frames_.size()) - 2) {
        throw StackCorruptionError("Broken dynamic link");
    }
    frames_.pop_back();
    return frame;
}

bool RuntimeStack::hasFrame() const { return !frames_.empty(); }

bool RuntimeStack::hasCallerFrame() const { return frames_.size() > 1; }

const StackFrame &RuntimeStack::currentFrame() const {
    if (frames_.empty()) {
        throw StackUnderflowError("No activation frame is active");
    }
    return frames_.back();
}

std::size_t RuntimeStack::frameCount() const { return frames_.size(); }

std::size_t RuntimeStack::maxFrameCount() const { return maxFrameCount_; }

std::size_t RuntimeStack::currentOperandBase() const {
    return currentFrame().operandBase;
}

int RuntimeStack::currentReturnAddress() const {
    return currentFrame().returnAddress;
}

void RuntimeStack::validateAddress(const RuntimeAddress &address) const {
    if (address.frameIndex < 0 ||
        address.frameIndex >= static_cast<int>(frames_.size())) {
        throw InvalidMemoryError("Address references an inactive frame");
    }
    const StackFrame &frame =
        frames_[static_cast<std::size_t>(address.frameIndex)];
    if (address.offset < kFrameHeaderSize || address.offset >= frame.slotCount) {
        throw InvalidMemoryError(
            "Runtime address is outside writable frame slots: " +
            std::to_string(address.offset));
    }
}
