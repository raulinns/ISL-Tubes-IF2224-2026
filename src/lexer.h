#ifndef LEXER_H
#define LEXER_H

#include "token.h"

#include <fstream>
#include <string>
#include <vector>

class Lexer {
  public:
    // Konstruktor, membuka file source code
    explicit Lexer(const std::string &filename);

    // Destruktor, tutup file
    ~Lexer();

    std::vector<Token> tokenize();

  private:
    std::ifstream src_; // Stream file source code
    char current_;      // Karakter yang sedang dibaca oleh DFA
    int line_;          // Nomor baris saat ini (untuk error reporting)

    char nextChar();
    void prevChar(char c);
    bool isEOF() const;

    Token readNumber();
    Token readStringOrChar();

    Token readIdentOrKeyword();

    // Fungsi pembantu ubah string ke lowercase untuk perbandingan case-insensitive
    static std::string toLower(const std::string &s);

    // Fungsi pembantu untuk keyword Arion.
    // Mengembalikan TokenType keyword jika ditemukan, atau IDENT jika tidak.
    static TokenType lookupKeyword(const std::string &lowerWord);

    Token readComment(char openChar);
    Token readOperatorOrPunct();
};

#endif // LEXER_H
