#include "lexer.h"
#include "token.h"
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>

// Konstruktor dan Destruktor

Lexer::Lexer(const std::string& filename)
    : current_('\0'), line_(1)
{
    src_.open(filename);
    if (!src_.is_open()) {
        throw std::runtime_error("Tidak dapat membuka file: " + filename);
    }
}

Lexer::~Lexer() {
    if (src_.is_open()) src_.close();
}

char Lexer::nextChar() {
    int ch = src_.get();
    if (ch == EOF) {
        current_ = '\0';
    } else {
        current_ = static_cast<char>(ch);
        if (current_ == '\n') ++line_;
    }
    return current_;
}

void Lexer::prevChar(char c) {
    // Kembaliin satu karakter ke stream (unget)
    if (c == '\n') --line_;
    src_.putback(c);
    current_ = c;
}

bool Lexer::isEOF() const {
    return src_.eof() || current_ == '\0';
}


std::string Lexer::toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return result;
}

TokenType Lexer::lookupKeyword(const std::string& lowerWord) {
}

// Kerjain ini buat masing-masing pembagian tugas
Token Lexer::readIdentOrKeyword() {}

Token Lexer::readNumber() {}

Token Lexer::readStringOrChar() {}

Token Lexer::readComment(char openChar) {}

Token Lexer::readOperatorOrPunct() {}

// Ini placeholder aja dan belom tentu bener
// Kerjain implementasi per orang dulu yang di readXXX
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    nextChar();

    while (!isEOF()) {
        char c = current_;

        if (std::isspace(static_cast<unsigned char>(c))) {
            nextChar();
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(c))) {
            tokens.push_back(readIdentOrKeyword());
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(readNumber());
            continue;
        }

        if (c == '\'') {
            tokens.push_back(readStringOrChar());
            continue;
        }

        tokens.push_back(readOperatorOrPunct());
}

    return tokens;
}
