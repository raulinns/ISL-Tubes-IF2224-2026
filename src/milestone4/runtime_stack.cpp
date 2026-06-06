#include "runtime_stack.h"

#include "arion_runtime_error.h"

#include <stdexcept>
#include <string>

RuntimeStack::RuntimeStack()
    : RuntimeStack(RuntimeStack::kDefaultMaxFrameCount) {}

RuntimeStack::RuntimeStack(std::size_t maxFrameCount)
    : maxFrameCount_(maxFrameCount), topLevelFloor_(0) {
    if (maxFrameCount_ == 0U) {
        throw std::invalid_argument("RuntimeStack maxFrameCount must be positive");
    }
}

void RuntimeStack::reset() {
    memory_.clear();
    frames_.clear();
    topLevelFloor_ = 0;
}

void RuntimeStack::reserveSlots(int slotCount) {
    if (slotCount < 0) {
        throw std::invalid_argument("RuntimeStack slot count cannot be negative");
    }

    if (frames_.empty()) {
        topLevelFloor_ = slotCount;
        resizeToAtLeast(slotCount);
        return;
    }

    StackFrame &frame = frames_.back();
    if (slotCount <= frame.slotCount) {
        frame.slotCount = slotCount;
        return;
    }

    const int oldFloor = currentFloor();
    const int addedSlots = slotCount - frame.slotCount;
    memory_.insert(memory_.begin() + oldFloor, static_cast<std::size_t>(addedSlots),
                   RuntimeValue());
    frame.slotCount = slotCount;
}

int RuntimeStack::allocateSlot() {
    const int address = currentFloor();

    if (frames_.empty()) {
        ++topLevelFloor_;
        resizeToAtLeast(topLevelFloor_);
    } else {
        ++frames_.back().slotCount;
        resizeToAtLeast(currentFloor());
    }

    return address;
}

bool RuntimeStack::hasAddress(int address) const {
    return address >= 0 && address < currentFloor();
}

const RuntimeValue &RuntimeStack::readAt(int address) const {
    validateAddress(address);
    return memory_[static_cast<std::size_t>(address)];
}

RuntimeValue &RuntimeStack::readAt(int address) {
    validateAddress(address);
    return memory_[static_cast<std::size_t>(address)];
}

void RuntimeStack::writeAt(int address, const RuntimeValue &value) {
    validateAddress(address);
    memory_[static_cast<std::size_t>(address)] = value;
}

int RuntimeStack::resolveAddress(int level, int address) const {
    if (level == 0) {
        return address;
    }
    if (level == 1) {
        if (frames_.empty()) {
            throw ArionRuntimeError(
                "Frame-relative address access requires an active stack frame");
        }
        return frames_.back().baseAddress + address;
    }
    throw ArionRuntimeError("Unsupported runtime address mode: " +
                            std::to_string(level));
}

const RuntimeValue &RuntimeStack::readAt(int level, int address) const {
    return readAt(resolveAddress(level, address));
}

void RuntimeStack::writeAt(int level, int address, const RuntimeValue &value) {
    writeAt(resolveAddress(level, address), value);
}

void RuntimeStack::pushValue(const RuntimeValue &value) {
    memory_.push_back(value);
}

RuntimeValue RuntimeStack::popValue() {
    if (memory_.size() <= static_cast<std::size_t>(currentFloor())) {
        throw StackUnderflowError(
            "Runtime stack underflow while popping an evaluation value");
    }

    RuntimeValue value = memory_.back();
    memory_.pop_back();
    return value;
}

const RuntimeValue &RuntimeStack::peekValue(std::size_t depth) const {
    const std::size_t availableValues =
        memory_.size() - static_cast<std::size_t>(currentFloor());
    if (depth >= availableValues) {
        throw StackUnderflowError(
            "Runtime stack underflow while peeking an evaluation value");
    }

    return memory_[memory_.size() - 1U - depth];
}

void RuntimeStack::pushFrame(const StackFrame &frame) {
    pushFrame(frame.baseAddress, frame.staticLink, frame.dynamicLink,
              frame.returnAddress, frame.slotCount);
}

void RuntimeStack::pushFrame(int baseAddress, int staticLink, int dynamicLink,
                             int returnAddress, int slotCount) {
    if (frames_.size() >= maxFrameCount_) {
        throw StackOverflowError(
            "Runtime stack frame limit exceeded while pushing a new frame");
    }
    if (baseAddress < 0 || slotCount < 0) {
        throw std::invalid_argument(
            "Stack frame base address and slot count must be non-negative");
    }
    if (baseAddress < currentFloor()) {
        throw ArionRuntimeError(
            "Stack frame base address overlaps existing allocated memory");
    }

    frames_.push_back(
        StackFrame{baseAddress, staticLink, dynamicLink, returnAddress,
                   slotCount});
    resizeToAtLeast(currentFloor());
}

StackFrame RuntimeStack::popFrame() {
    if (frames_.empty()) {
        throw StackUnderflowError(
            "Runtime stack underflow while popping a stack frame");
    }

    const StackFrame frame = frames_.back();
    frames_.pop_back();
    memory_.resize(static_cast<std::size_t>(frame.baseAddress));
    return frame;
}

bool RuntimeStack::hasFrame() const { return !frames_.empty(); }

const StackFrame &RuntimeStack::currentFrame() const {
    if (frames_.empty()) {
        throw StackUnderflowError("Runtime stack does not have an active frame");
    }
    return frames_.back();
}

int RuntimeStack::currentBaseAddress() const {
    return frames_.empty() ? 0 : frames_.back().baseAddress;
}

int RuntimeStack::currentFloorAddress() const { return currentFloor(); }

std::size_t RuntimeStack::frameCount() const { return frames_.size(); }

std::size_t RuntimeStack::maxFrameCount() const { return maxFrameCount_; }

int RuntimeStack::memorySize() const {
    return static_cast<int>(memory_.size());
}

const std::vector<RuntimeValue> &RuntimeStack::memory() const { return memory_; }

int RuntimeStack::currentFloor() const {
    if (frames_.empty()) {
        return topLevelFloor_;
    }
    return frames_.back().baseAddress + frames_.back().slotCount;
}

void RuntimeStack::resizeToAtLeast(int slotCount) {
    if (slotCount < 0) {
        throw std::invalid_argument("RuntimeStack size cannot be negative");
    }

    if (memory_.size() < static_cast<std::size_t>(slotCount)) {
        memory_.resize(static_cast<std::size_t>(slotCount));
    }
}

void RuntimeStack::validateAddress(int address) const {
    if (!hasAddress(address)) {
        throw ArionRuntimeError("Invalid runtime address access: " +
                                std::to_string(address));
    }
}
