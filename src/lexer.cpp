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
    // Menerima string lowercase, mengembalikan TokenType keyword kontrol yang sesuai. Jika tidak ada yang cocok, return IDENT.
    if (lowerWord == "if")      return IFSY;
    if (lowerWord == "case")    return CASESY;
    if (lowerWord == "repeat")  return REPEATSY;
    if (lowerWord == "while")   return WHILESY;
    if (lowerWord == "for")     return FORSY;
    if (lowerWord == "until")   return UNTILSY;
    if (lowerWord == "of")      return OFSY;
    if (lowerWord == "do")      return DOSY;
    if (lowerWord == "to")      return TOSY;
    if (lowerWord == "downto")  return DOWNTOSY;
    if (lowerWord == "then")    return THENSY;
    if (lowerWord == "begin")   return BEGINSY;
    if (lowerWord == "end")     return ENDSY;
    if (lowerWord == "else")    return ELSESY;

    return IDENT;
}

// Baca 2 tipe, tipe 1 : { ... } and tipe 2 : (* ... *)
// Parameter openChar: { = tipe 1, caller udah baca { sama ( = tipe 2, caller udah baca ( sama *

Token Lexer::readComment(char openChar) {
    std::string body;
    int startLine = line_;

    if (openChar == '{') {
        // Tipe 1
        // State: S_CMT1
        char c = nextChar();
        while (c != '}' && c != '\0') {
            body += c;
            c = nextChar();
        }
        if (c == '}') {
            return Token(COMMENT, "{" + body + "}", startLine);
        }
        return Token(TOKEN_ERROR, "Komentar '{' tidak ditutup", startLine);

    } else {
        // Tipe 2. ( dan * udah tadi sama readOperatorOrPunct
        // State awal: S_CMT2
        char c = nextChar();
        while (true) {
            if (c == '\0') {
                return Token(TOKEN_ERROR, "Komentar '(*' tidak ditutup", startLine);
            }
            if (c == '*') {
                // State: S_CMT2_STAR
                char next = nextChar();
                if (next == ')') {
                    return Token(COMMENT, "(*" + body + "*)", startLine);
                }
                if (next == '*') {
                    // teteep di S_CMT2_STAR
                    body += c;
                    c = next;
                    continue;
                }
                // Balik ke S_CMT2
                body += c;
                c = next;
                continue;
            }
            body += c;
            c = nextChar();
        }
    }
}

// Kerjain ini buat masing-masing pembagian tugas
Token Lexer::readIdentOrKeyword() {}

Token Lexer::readNumber() {}

Token Lexer::readStringOrChar() {}

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
