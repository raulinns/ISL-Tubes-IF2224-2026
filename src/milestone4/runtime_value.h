#ifndef RUNTIME_VALUE_H
#define RUNTIME_VALUE_H

#include <string>
#include <variant>

enum class RuntimeValueKind {
    Uninitialized,
    Integer,
    Real,
    Boolean,
    Char,
    String,
    Address
};

struct RuntimeAddress {
    int frameIndex;
    int offset;
};

std::string runtimeValueKindToString(RuntimeValueKind kind);

class RuntimeValue {
  public:
    RuntimeValue();

    static RuntimeValue makeInteger(int value);
    static RuntimeValue makeReal(double value);
    static RuntimeValue makeBoolean(bool value);
    static RuntimeValue makeChar(char value);
    static RuntimeValue makeString(std::string value);
    static RuntimeValue makeAddress(int frameIndex, int offset);
    static RuntimeValue defaultValue(RuntimeValueKind kind);
    static RuntimeValue parseLiteral(const std::string &text,
                                     const std::string &declaredType = "");

    RuntimeValueKind kind() const;
    bool isInitialized() const;
    bool isNumeric() const;

    int asInteger() const;
    double asReal() const;
    bool asBoolean() const;
    char asChar() const;
    const std::string &asString() const;
    RuntimeAddress asAddress() const;

    std::string toString() const;

  private:
    using Storage =
        std::variant<std::monostate, int, double, bool, char, std::string,
                     RuntimeAddress>;

    explicit RuntimeValue(Storage storage);

    Storage value_;
};

#endif // RUNTIME_VALUE_H
