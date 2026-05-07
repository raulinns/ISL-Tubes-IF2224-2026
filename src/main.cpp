#include "lexer.h"
#include "parse_tree.h"
#include "parser.h"
#include "token.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// printUsage: menampilkan cara pemakaian program ke stderr
static void printUsage(const char *programName) {
    std::cerr << "Penggunaan: " << programName << " <input.txt> [output.txt]\n"
              << "  input.txt   : file source code bahasa Arion\n"
              << "  output.txt  : (opsional) file output parse tree\n"
              << "                Jika tidak diberikan, parse tree output ke terminal\n";
}

// reportErrors: mencetak ringkasan token error ke stderr setelah tokenisasi
static void reportErrors(const std::vector<Token> &tokens) {
    bool hasError = false;
    for (const auto &tok : tokens) {
        if (tok.type == TOKEN_ERROR) {
            if (!hasError) {
                std::cerr
                    << "\n[LEXER] Ditemukan karakter/token tidak dikenal:\n";
                hasError = true;
            }
            std::cerr << "  Baris " << tok.line << ": karakter '" << tok.value
                      << "'\n";
        }
    }
    if (hasError) {
        std::cerr << "[LEXER] Proses tokenisasi tetap dilanjutkan.\n\n";
    }
}

static bool hasLexicalError(const std::vector<Token> &tokens) {
    for (const auto &tok : tokens) {
        if (tok.type == TOKEN_ERROR) {
            return true;
        }
    }
    return false;
}

// main
int main(int argc, char *argv[]) {
    // 1. Validasi argumen
    if (argc < 2 || argc > 3) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    const std::string inputFile = argv[1];
    const std::string outputFile = (argc == 3) ? argv[2] : "";

    // 2. Jalankan Lexer
    std::vector<Token> tokens;

    try {
        Lexer lexer(inputFile);
        tokens = lexer.tokenize();
    } catch (const std::runtime_error &e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] Exception tidak terduga: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    // 3. Laporkan error leksikal (jika ada) ke stderr
    reportErrors(tokens);

    if (hasLexicalError(tokens)) {
        std::cerr << "[ERROR] Parsing dibatalkan karena masih ada token "
                     "leksikal tidak valid.\n";
        return EXIT_FAILURE;
    }

    std::string parseTree;
    try {
        Parser parser(tokens);
        parseTree = renderParseTree(parser.parseProgram());
    } catch (const std::runtime_error &e) {
        std::cerr << "[PARSER] " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception &e) {
        std::cerr << "[PARSER] Exception tidak terduga: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    // 4. Tulis output
    if (outputFile.empty()) {
        std::cout << "----- Parse Tree -----\n\n";
        std::cout << parseTree;
    } else {
        std::ofstream out(outputFile);
        if (!out.is_open()) {
            std::cerr << "[ERROR] Tidak dapat membuat file output: "
                      << outputFile << "\n";
            return EXIT_FAILURE;
        }

        out << parseTree;
        out.close();

        std::cout << "[OK] Parsing selesai. Parse tree ditulis ke '"
                  << outputFile << "'\n";
    }

    return EXIT_SUCCESS;
}
