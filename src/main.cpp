#include "ast.h"
#include "ast_builder.h"
#include "lexer.h"
#include "parse_tree.h"
#include "parser.h"
#include "semantic_analyzer.h"
#include "milestone4/code_generator.h"
#include "milestone4/intermediate_code.h"
#include "milestone4/interpreter.h"
#include "token.h"

#include <cstdlib>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// printUsage: menampilkan cara pemakaian program ke stderr
static void printUsage(const char *programName) {
    std::cerr << "Penggunaan: " << programName << " <input.txt> [output.txt]\n"
              << "  input.txt   : file source code bahasa Arion\n"
              << "  output.txt  : (opsional) file output analisis\n"
              << "                Jika tidak diberikan, output analisis ke terminal\n";
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

static std::string formatSemanticDiagnostics(const std::vector<SemanticDiagnostic> &diagnostics) {
    if (diagnostics.empty()) {
        return "No semantic diagnostics.\n";
    }

    std::ostringstream out;
    for (const SemanticDiagnostic &diagnostic : diagnostics) {
        out << (diagnostic.severity == SemanticDiagnostic::Severity::Error
                    ? "error"
                    : "warning")
            << ": " << diagnostic.message << '\n';
    }
    return out.str();
}

static std::string readWholeFile(const std::string &path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("Tidak dapat membuka file: " + path);
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

static std::string ltrimCopy(std::string text) {
    std::size_t pos = 0;
    while (pos < text.size() &&
           std::isspace(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }
    return text.substr(pos);
}

static bool looksLikeParseTreeInput(const std::string &text) {
    const std::string trimmed = ltrimCopy(text);
    return trimmed.rfind("<program>", 0) == 0;
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

    ParseNode parseTreeNode("<empty>");
    AstNode ast(AstKind::Program);
    SemanticResult semantic{AstNode(AstKind::Program), SymbolTable(), {}};

    try {
        const std::string inputText = readWholeFile(inputFile);
        if (looksLikeParseTreeInput(inputText)) {
            parseTreeNode = parseRenderedParseTree(inputText);
        } else {
            // 2. Jalankan Lexer
            Lexer lexer(inputFile);
            std::vector<Token> tokens = lexer.tokenize();

            // 3. Laporkan error leksikal (jika ada) ke stderr
            reportErrors(tokens);

            if (hasLexicalError(tokens)) {
                std::cerr << "[ERROR] Parsing dibatalkan karena masih ada token "
                             "leksikal tidak valid.\n";
                return EXIT_FAILURE;
            }

            Parser parser(tokens);
            parseTreeNode = parser.parseProgram();
        }
    } catch (const std::runtime_error &e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] Exception tidak terduga: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    try {
        ast = buildAstFromParseTree(parseTreeNode);
    } catch (const std::exception &e) {
        std::cerr << "[AST] " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    
    try {
        semantic = analyzeSemantics(std::move(ast));
    } catch (const std::exception &e) {
        std::cerr << "[SEMANTIC] " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    bool milestone4Ok = true;
    std::ostringstream output;
    output << "----- Decorated AST -----\n\n";
    output << renderAst(semantic.ast);

    output << "\n----- Symbol Table -----\n\n";
    output << semantic.symbols.renderAll();

    output << "\n----- Semantic Diagnostics -----\n\n";
    output << formatSemanticDiagnostics(semantic.diagnostics);

    if (semantic.ok()) {
        try {
            const CodeGeneratorResult generated =
                generateIntermediateCode(semantic.ast, semantic.symbols);
            output << "\n----- Intermediate Code -----\n\n";
            output << renderIntermediateCode(generated.code);

            const InterpreterResult interpreted =
                runIntermediateCode(generated.code);
            output << "\n----- Program Output -----\n\n";
            output << interpreted.output;
        } catch (const std::exception &e) {
            milestone4Ok = false;
            output << "\n----- Milestone 4 Error -----\n\n";
            output << e.what() << '\n';
        }
    }

    const std::string outputText = output.str();

    if (outputFile.empty()) {
        std::cout << outputText;
    } else {
        std::ofstream out(outputFile);
        if (!out.is_open()) {
            std::cerr << "[ERROR] Tidak dapat membuat file output: "
                      << outputFile << "\n";
            return EXIT_FAILURE;
        }

        out << outputText;
        out.close();

        if (semantic.ok()) {
            std::cout << "[OK] Analisis selesai. Output ditulis ke '"
                      << outputFile << "'\n";
        } else {
            std::cerr << "[SEMANTIC] Analisis selesai dengan error. Output ditulis ke '"
                      << outputFile << "'\n";
        }
    }

    return semantic.ok() && milestone4Ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
