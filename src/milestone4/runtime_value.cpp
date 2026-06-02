#include "runtime_value.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace {

std::string lowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::string decodeQuotedText(const std::string &text) {
    if (text.size() < 2 || text.front() != '\'' || text.back() != '\'') {
        throw std::invalid_argument("Quoted literal must be wrapped in single quotes");
    }

    std::string decoded;
    for (std::size_t i = 1; i + 1 < text.size(); ++i) {
        if (text[i] == '\'' && i + 1 < text.size() - 1 && text[i + 1] == '\'') {
            decoded.push_back('\'');
            ++i;
            continue;
        }
        decoded.push_back(text[i]);
    }
    return decoded;
}

} // namespace

std::string runtimeValueKindToString(RuntimeValueKind kind) {
    switch (kind) {
    case RuntimeValueKind::Uninitialized:
        return "uninitialized";
    case RuntimeValueKind::Integer:
        return "integer";
    case RuntimeValueKind::Real:
        return "real";
    case RuntimeValueKind::Boolean:
        return "boolean";
    case RuntimeValueKind::Char:
        return "char";
    case RuntimeValueKind::String:
        return "string";
    }

    throw std::invalid_argument("Unknown runtime value kind");
}

RuntimeValue::RuntimeValue() : value_(std::monostate()) {}

RuntimeValue::RuntimeValue(Storage storage) : value_(std::move(storage)) {}

RuntimeValue RuntimeValue::makeInteger(int value) { return RuntimeValue(value); }

RuntimeValue RuntimeValue::makeReal(double value) { return RuntimeValue(value); }

RuntimeValue RuntimeValue::makeBoolean(bool value) { return RuntimeValue(value); }

RuntimeValue RuntimeValue::makeChar(char value) { return RuntimeValue(value); }

RuntimeValue RuntimeValue::makeString(std::string value) {
    return RuntimeValue(std::move(value));
}

RuntimeValue RuntimeValue::defaultValue(RuntimeValueKind kind) {
    switch (kind) {
    case RuntimeValueKind::Uninitialized:
        return RuntimeValue();
    case RuntimeValueKind::Integer:
        return makeInteger(0);
    case RuntimeValueKind::Real:
        return makeReal(0.0);
    case RuntimeValueKind::Boolean:
        return makeBoolean(false);
    case RuntimeValueKind::Char:
        return makeChar('\0');
    case RuntimeValueKind::String:
        return makeString("");
    }

    throw std::invalid_argument("Unknown runtime value kind");
}

RuntimeValue RuntimeValue::parseLiteral(const std::string &text,
                                        const std::string &declaredType) {
    const std::string loweredType = lowerCopy(declaredType);
    const std::string loweredText = lowerCopy(text);

    if (loweredType == "boolean" || loweredText == "true" ||
        loweredText == "false") {
        if (loweredText == "true") {
            return makeBoolean(true);
        }
        if (loweredText == "false") {
            return makeBoolean(false);
        }
        throw std::invalid_argument("Boolean literal must be true or false");
    }

    if (loweredType == "char") {
        const std::string decoded = decodeQuotedText(text);
        if (decoded.size() != 1) {
            throw std::invalid_argument("Char literal must contain exactly one character");
        }
        return makeChar(decoded[0]);
    }

    if (loweredType == "string") {
        return makeString(decodeQuotedText(text));
    }

    if (!text.empty() && text.front() == '\'' && text.back() == '\'') {
        const std::string decoded = decodeQuotedText(text);
        return decoded.size() == 1 ? makeChar(decoded[0]) : makeString(decoded);
    }

    if (text.find('.') != std::string::npos || loweredType == "real") {
        std::size_t parsed = 0;
        const double value = std::stod(text, &parsed);
        if (parsed != text.size()) {
            throw std::invalid_argument("Invalid real literal: " + text);
        }
        return makeReal(value);
    }

    std::size_t parsed = 0;
    const int value = std::stoi(text, &parsed);
    if (parsed != text.size()) {
        throw std::invalid_argument("Invalid integer literal: " + text);
    }
    return makeInteger(value);
}

RuntimeValueKind RuntimeValue::kind() const {
    if (std::holds_alternative<std::monostate>(value_)) {
        return RuntimeValueKind::Uninitialized;
    }
    if (std::holds_alternative<int>(value_)) {
        return RuntimeValueKind::Integer;
    }
    if (std::holds_alternative<double>(value_)) {
        return RuntimeValueKind::Real;
    }
    if (std::holds_alternative<bool>(value_)) {
        return RuntimeValueKind::Boolean;
    }
    if (std::holds_alternative<char>(value_)) {
        return RuntimeValueKind::Char;
    }
    return RuntimeValueKind::String;
}

bool RuntimeValue::isInitialized() const {
    return kind() != RuntimeValueKind::Uninitialized;
}

bool RuntimeValue::isNumeric() const {
    return kind() == RuntimeValueKind::Integer ||
           kind() == RuntimeValueKind::Real;
}

int RuntimeValue::asInteger() const {
    if (!std::holds_alternative<int>(value_)) {
        throw std::runtime_error("Runtime value is not an integer");
    }
    return std::get<int>(value_);
}

double RuntimeValue::asReal() const {
    if (std::holds_alternative<double>(value_)) {
        return std::get<double>(value_);
    }
    if (std::holds_alternative<int>(value_)) {
        return static_cast<double>(std::get<int>(value_));
    }
    throw std::runtime_error("Runtime value is not numeric");
}

bool RuntimeValue::asBoolean() const {
    if (!std::holds_alternative<bool>(value_)) {
        throw std::runtime_error("Runtime value is not a boolean");
    }
    return std::get<bool>(value_);
}

char RuntimeValue::asChar() const {
    if (!std::holds_alternative<char>(value_)) {
        throw std::runtime_error("Runtime value is not a char");
    }
    return std::get<char>(value_);
}

const std::string &RuntimeValue::asString() const {
    if (!std::holds_alternative<std::string>(value_)) {
        throw std::runtime_error("Runtime value is not a string");
    }
    return std::get<std::string>(value_);
}

std::string RuntimeValue::toString() const {
    std::ostringstream out;
    switch (kind()) {
    case RuntimeValueKind::Uninitialized:
        return "<uninitialized>";
    case RuntimeValueKind::Integer:
        out << std::get<int>(value_);
        return out.str();
    case RuntimeValueKind::Real:
        out << std::get<double>(value_);
        return out.str();
    case RuntimeValueKind::Boolean:
        return std::get<bool>(value_) ? "true" : "false";
    case RuntimeValueKind::Char:
        return std::string(1, std::get<char>(value_));
    case RuntimeValueKind::String:
        return std::get<std::string>(value_);
    }

    throw std::invalid_argument("Unknown runtime value kind");
}
