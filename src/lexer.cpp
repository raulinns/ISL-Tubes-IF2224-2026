#include "lexer.h"
#include "token.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

// Konstruktor dan Destruktor

Lexer::Lexer(const std::string &filename) : current_('\0'), line_(1) {
    src_.open(filename);
    if (!src_.is_open()) {
        throw std::runtime_error("Tidak dapat membuka file: " + filename);
    }
}

Lexer::~Lexer() {
    if (src_.is_open())
        src_.close();
}

char Lexer::nextChar() {
    int ch = src_.get();
    if (ch == EOF) {
        current_ = '\0';
    } else {
        current_ = static_cast<char>(ch);
        if (current_ == '\n')
            ++line_;
    }
    return current_;
}

void Lexer::prevChar(char c) {
    // Kembalikan satu karakter ke stream (unget)
    if (c == '\n')
        --line_;
    src_.putback(c);
    current_ = c;
}

bool Lexer::isEOF() const { return src_.eof() || current_ == '\0'; }

std::string Lexer::toLower(const std::string &s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// =============================================================================
// lookupKeyword
//
// Menerima string huruf kecil, mengembalikan TokenType yang sesuai.
//
// ── Keywords yang diimplementasikan di sini ──────────────────────────────────
//   Kontrol alur  : if, case, repeat, while, for, until, of, do, to, downto,
//                   then, begin, end, else
//   Deklarasi     : const, type, var, function, procedure, array, record,
//   program Operator kata : not, and, or, div, mod
//
// Jika tidak ada yang cocok → IDENT
// =============================================================================

TokenType Lexer::lookupKeyword(const std::string &lowerWord) {}

// Kerjain ini buat masing-masing pembagian tugas
Token Lexer::readIdentOrKeyword() {
    std::string word;
    int startLine = line_;

    while (!isEOF() && (std::isalnum(static_cast<unsigned char>(current_)) ||
                        current_ == '_')) {
        word += current_;
        nextChar();
    }

    if (!isEOF())
        prevChar(current_);

    TokenType type = lookupKeyword(toLower(word));

    if (type == IDENT) {
        return Token(IDENT, word, startLine);
    } else {
        return Token(type, "", startLine);
    }
}

Token Lexer::readNumber() {
    std::string numStr;
    int startLine = line_;

    while (!isEOF() && std::isdigit(static_cast<unsigned char>(current_))) {
        numStr += current_;
        nextChar();
    }

    if (!isEOF() && current_ == '.') {
        char dot = current_;
        nextChar();
        if (!isEOF() && std::isdigit(static_cast<unsigned char>(current_))) {
            numStr += dot;

            while (!isEOF() &&
                   std::isdigit(static_cast<unsigned char>(current_))) {
                numStr += current_;
                nextChar();
            }

            if (!isEOF())
                prevChar(current_);
            return Token(REALCON, numStr, startLine);

        } else {
            numStr += dot;
            if (!isEOF())
                prevChar(current_);
            return Token(TOKEN_ERROR, numStr, startLine);
        }
    }

    if (!isEOF())
        prevChar(current_);
    return Token(INTCON, numStr, startLine);
}

Token Lexer::readStringOrChar() {
    std::string content;
    int startLine = line_;

    nextChar();

    while (true) {
        if (isEOF()) {
            return Token(TOKEN_ERROR,
                         "unterminated string/char at line " +
                             std::to_string(startLine),
                         startLine);
        }

        if (current_ == '\'') {
            nextChar();

            if (!isEOF() && current_ == '\'') {
                content += '\''; // escaped quote ''
                nextChar();
            } else {
                if (!isEOF())
                    prevChar(current_);
                break;
            }
        } else {
            content += current_;
            nextChar();
        }
    }

    if (content.size() == 1) {
        return Token(CHARCON, content, startLine);
    } else {
        return Token(STRING, content, startLine);
    }
}

Token Lexer::readComment(char openChar) {}

Token Lexer::readOperatorOrPunct() {
    int startLine = line_;
    char c = current_;
    switch (c) {
    case '+':
        return Token(PLUS, "", startLine);
    case '-':
        return Token(MINUS, "", startLine);
    case '*':
        return Token(TIMES, "", startLine);
    case '/':
        return Token(RDIV, "", startLine);
    default:
        return Token(TOKEN_ERROR, std::string(1, c), startLine);
    }
}

// Ini placeholder aja, dari AI (belum tentu bener), recheck nanti aja
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
            nextChar();
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(readNumber());
            nextChar();
            continue;
        }

        if (c == '\'') {
            tokens.push_back(readStringOrChar());
            nextChar();
            continue;
        }

        tokens.push_back(readOperatorOrPunct());
        nextChar();
    }

    return tokens;
}
