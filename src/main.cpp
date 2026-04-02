#include "lexer.h"
#include "token.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// printUsage: menampilkan cara pemakaian program ke stderr
static void printUsage(const char* programName) {
    std::cerr << "Penggunaan: " << programName << " <input.txt> [output.txt]\n" << "  input.txt   : file source code bahasa Arion\n" << "  output.txt  : (opsional) file output daftar token\n" << "                Jika tidak diberikan, output ke terminal\n";
}

// formatToken: mengonversi satu Token menjadi string output sesuai spesifikasi
// Token tanpa nilai = "tokenname"; contoh: semicolon
// Token dengan nilai = "tokenname (value)"; contoh: intcon (5)
// contoh: ident (Hello)
static std::string formatToken(const Token& tok) {
    std::string name = tokenTypeToString(tok.type);

    if (tokenNeedsValue(tok.type) && !tok.value.empty()) {
        return name + " (" + tok.value + ")";
    }
    return name;
}

// writeTokens: menulis daftar token ke stream output (terminal atau file)
// - Baris kosong disisipkan setelah token yang menandai akhir "bagian" besar, yaitu setelah PROGRAMSY, SEMICOLON (pada baris deklarasi), d;;.
// ! Pisah sama newline tunggal per token; baris kosong disisipkan setelah SEMICOLON untuk keterbacaan.
static void writeTokens(const std::vector<Token>& tokens, std::ostream& out) {
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        out << formatToken(tokens[i]) << "\n";

        // Sisipkan baris kosong setelah SEMICOLON agar output lebih mudah dibaca
        // (mengikuti gaya pada contoh output spesifikasi)
        if (tokens[i].type == SEMICOLON && i + 1 < tokens.size()) {
            // Hanya sisipkan jika token berikutnya bukan SEMICOLON
            if (tokens[i + 1].type != SEMICOLON) {
                out << "\n";
            }
        }
    }
}

// reportErrors: mencetak ringkasan token error ke stderr setelah tokenisasi
static void reportErrors(const std::vector<Token>& tokens) {
    bool hasError = false;
    for (const auto& tok : tokens) {
        if (tok.type == TOKEN_ERROR) {
            if (!hasError) {
                std::cerr << "\n[LEXER] Ditemukan karakter/token tidak dikenal:\n";
                hasError = true;
            }
            std::cerr << "  Baris " << tok.line
                      << ": karakter '" << tok.value << "'\n";
        }
    }
    if (hasError) {
        std::cerr << "[LEXER] Proses tokenisasi tetap dilanjutkan.\n\n";
    }
}

// main
int main(int argc, char* argv[]) {
    // 1. Validasi argumen
    if (argc < 2 || argc > 3) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    const std::string inputFile  = argv[1];
    const std::string outputFile = (argc == 3) ? argv[2] : "";

    // 2. Jalankan Lexer
    std::vector<Token> tokens;

    try {
        Lexer lexer(inputFile);
        tokens = lexer.tokenize();
    }
    catch (const std::runtime_error& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception tidak terduga: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    // 3. Laporkan error leksikal (jika ada) ke stderr
    reportErrors(tokens);

    // 4. Filter: buang token COMMENT dari output
    // Komentar sudah dikenali lexer (untuk error-checking), tapi tidak
    // diteruskan ke tahap selanjutnya (parsing), sesuai perilaku lexer standar.
    std::vector<Token> filtered;
    filtered.reserve(tokens.size());
    for (const auto& tok : tokens) {
        if (tok.type != COMMENT) {
            filtered.push_back(tok);
        }
    }

    // 5. Tulis output
    if (outputFile.empty()) {
        // Tidak ada argumen output, cetak ke terminal (stdout)
        std::cout << "----- Daftar Token -----\n\n";
        writeTokens(filtered, std::cout);
        std::cout << "\n----- Total: " << filtered.size() << " token -----\n";
    }
    else {
        // Ada argumen output → tulis ke file .txt
        std::ofstream out(outputFile);
        if (!out.is_open()) {
            std::cerr << "[ERROR] Tidak dapat membuat file output: " << outputFile << "\n";
            return EXIT_FAILURE;
        }

        writeTokens(filtered, out);
        out.close();

        // Konfirmasi ke terminal
        std::cout << "[OK] Tokenisasi selesai. " << filtered.size() << " token ditulis ke '" << outputFile << "'\n";
    }

    return EXIT_SUCCESS;
}
