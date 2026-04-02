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
  if (src_.is_open()) {
    src_.close();
  }
}

char Lexer::nextChar() {
  int ch = src_.get();
  if (ch == EOF) {
    current_ = '\0';
  } else {
    current_ = static_cast<char>(ch);
    if (current_ == '\n') {
      ++line_;
    }
  }
  return current_;
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

TokenType Lexer::lookupKeyword(const std::string &lowerWord) {
  // Menerima string lowercase, mengembalikan TokenType keyword kontrol yang
  // sesuai. Jika tidak ada yang cocok, return IDENT.
  if (lowerWord == "if")
    return IFSY;
  if (lowerWord == "case")
    return CASESY;
  if (lowerWord == "repeat")
    return REPEATSY;
  if (lowerWord == "while")
    return WHILESY;
  if (lowerWord == "for")
    return FORSY;
  if (lowerWord == "until")
    return UNTILSY;
  if (lowerWord == "of")
    return OFSY;
  if (lowerWord == "do")
    return DOSY;
  if (lowerWord == "to")
    return TOSY;
  if (lowerWord == "downto")
    return DOWNTOSY;
  if (lowerWord == "then")
    return THENSY;
  if (lowerWord == "begin")
    return BEGINSY;
  if (lowerWord == "end")
    return ENDSY;
  if (lowerWord == "else")
    return ELSESY;
  if (lowerWord == "not")
    return NOTSY;
  if (lowerWord == "and")
    return ANDSY;
  if (lowerWord == "or")
    return ORSY;
  if (lowerWord == "div")
    return IDIV;
  if (lowerWord == "mod")
    return IMOD;
  if (lowerWord == "const")
    return CONSTSY;
  if (lowerWord == "type")
    return TYPESY;
  if (lowerWord == "var")
    return VARSY;
  if (lowerWord == "function")
    return FUNCTIONSY;
  if (lowerWord == "procedure")
    return PROCEDURESY;
  if (lowerWord == "array")
    return ARRAYSY;
  if (lowerWord == "record")
    return RECORDSY;
  if (lowerWord == "program")
    return PROGRAMSY;

  return IDENT;
}

Token Lexer::readIdentOrKeyword() {
  std::string word;
  int startLine = line_;

  while (!isEOF() && std::isalnum(static_cast<unsigned char>(current_))) {
    word += current_;
    nextChar();
  }

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

      while (!isEOF() && std::isdigit(static_cast<unsigned char>(current_))) {
        numStr += current_;
        nextChar();
      }
      return Token(REALCON, numStr, startLine);

    } else {
      numStr += dot;
      return Token(TOKEN_ERROR, numStr, startLine);
    }
  }
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
        break;
      }
    } else {
      content += current_;
      nextChar();
    }
  }

  if (content.size() == 1) {
    return Token(CHARCON, "'" + content + "'", startLine);
  } else {
    return Token(STRING, "'" + content + "'", startLine);
  }
}

Token Lexer::readOperatorOrPunct() {
  int startLine = line_;
  char c = current_;
  switch (c) {
  case '+':
    nextChar();
    return Token(PLUS, "", startLine);
  case '-':
    nextChar();
    return Token(MINUS, "", startLine);
  case '*':
    nextChar();
    return Token(TIMES, "", startLine);
  case '/':
    nextChar();
    return Token(RDIV, "", startLine);
  case '{':
    nextChar();
    return readComment('{');
  case '(':
    nextChar();
    if (!isEOF() && current_ == '*') {
      nextChar();
      return readComment('(');
    }
    return Token(LPARENT, "", startLine);
  case ')':
    nextChar();
    return Token(RPARENT, "", startLine);
  case '[':
    nextChar();
    return Token(LBRACK, "", startLine);
  case ']':
    nextChar();
    return Token(RBRACK, "", startLine);
  case ',':
    nextChar();
    return Token(COMMA, "", startLine);
  case ';':
    nextChar();
    return Token(SEMICOLON, "", startLine);
  case '.':
    nextChar();
    return Token(PERIOD, "", startLine);
  case ':': {
    int nxt = src_.peek();
    if (nxt == '=') {
      nextChar();
      nextChar();
      return Token(BECOMES, "", startLine);
    }
    nextChar();
    return Token(COLON, "", startLine);
  }

  case '>': {
    char lookahead = nextChar();
    if (lookahead == '=') {
      nextChar();
      return Token(GEQ, "", startLine);
    }
    return Token(GTR, "", startLine);
  }

  case '<': {
    char lookahead = nextChar();
    if (lookahead == '=') {
      nextChar();
      return Token(LEQ, "", startLine);
    }
    if (lookahead == '>') {
      nextChar();
      return Token(NEQ, "", startLine);
    }
    return Token(LSS, "", startLine);
  }

  case '=': {
    char lookahead = nextChar();
    if (lookahead == '=') {
      nextChar();
      return Token(EQL, "", startLine);
    }
    return Token(TOKEN_ERROR, std::string(1, c), startLine);
  }

  default:
    nextChar();
    return Token(TOKEN_ERROR, std::string(1, c), startLine);
  }
}

// Baca 2 tipe, tipe 1 : { ... } and tipe 2 : (* ... *)
// Parameter openChar: { = tipe 1, caller udah baca { sama ( = tipe 2, caller
// udah baca ( sama *

Token Lexer::readComment(char openChar) {
  std::string body;
  int startLine = line_;

  if (openChar == '{') {
    // Tipe 1
    // State: qCB
    // current_ sudah berisi char pertama setelah '{' (di-advance oleh caller)
    char c = current_;
    while (c != '}' && c != '\0') {
      body += c;
      c = nextChar();
    }

    // qCB + '}' -> accept, masuk COMMENT
    // qCB + EOF -> TOKEN_ERROR
    if (c == '}') {
      nextChar();
      return Token(COMMENT, "{" + body + "}", startLine);
    }
    return Token(TOKEN_ERROR, "Komentar '{' tidak ditutup", startLine);

  } else {
    // Tipe 2. ( dan * udah tadi sama readOperatorOrPunct
    // State awal: qCP1
    // current_ sudah berisi char pertama setelah '*' (di-advance oleh caller)
    char c = current_;
    while (true) {

      // qCP1 + EOF -> TOKEN_ERROR
      // qCP1 + '*' -> pindah ke qCP2
      // qCP1 + lainnya -> tetep qCP1 (loop)
      if (c == '\0') {
        return Token(TOKEN_ERROR, "Komentar '(*' tidak ditutup", startLine);
      }
      if (c == '*') {
        // State: qCP2
        char next = nextChar();

        // qCP2 + ')' -> accept masuk COMMENT
        // qCP2 + '*' -> tetep qCP2
        // qCP2 + lainnya -> balik qCP1
        if (next == ')') {
          nextChar();
          return Token(COMMENT, "(*" + body + "*)", startLine);
        }
        if (next == '*') {
          // teteep di qCP2
          body += c;
          c = next;
          continue;
        }
        // Balik ke qCP1
        body += c;
        c = next;
        continue;
      }
      body += c;
      c = nextChar();
    }
  }
}

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
