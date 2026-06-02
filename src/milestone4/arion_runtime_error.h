#ifndef ARION_RUNTIME_ERROR_H
#define ARION_RUNTIME_ERROR_H

#include <stdexcept>
#include <string>

class ArionRuntimeError : public std::runtime_error {
  public:
    explicit ArionRuntimeError(const std::string &message)
        : std::runtime_error(message) {}
};

class StackOverflowError : public ArionRuntimeError {
  public:
    explicit StackOverflowError(const std::string &message)
        : ArionRuntimeError(message) {}
};

class StackUnderflowError : public ArionRuntimeError {
  public:
    explicit StackUnderflowError(const std::string &message)
        : ArionRuntimeError(message) {}
};

class InvalidJumpTargetError : public ArionRuntimeError {
  public:
    explicit InvalidJumpTargetError(const std::string &message)
        : ArionRuntimeError(message) {}
};

class DivisionByZeroError : public ArionRuntimeError {
  public:
    explicit DivisionByZeroError(const std::string &message)
        : ArionRuntimeError(message) {}
};

class RuntimeTypeError : public ArionRuntimeError {
  public:
    explicit RuntimeTypeError(const std::string &message)
        : ArionRuntimeError(message) {}
};

#endif // ARION_RUNTIME_ERROR_H
