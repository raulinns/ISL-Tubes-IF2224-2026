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

namespace {

// printUsage: menampilkan cara pemakaian program ke stderr
static void printUsage(const char *programName) {
    std::cerr << "Penggunaan: " << programName << " <input.txt> [output.txt]\n"
              << "  input.txt   : source code Arion atau laporan Decorated AST\n"
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

static bool looksLikeDecoratedAstInput(const std::string &text) {
    const std::string trimmed = ltrimCopy(text);
    return trimmed.rfind("Program(", 0) == 0 ||
           trimmed.rfind("----- Decorated AST -----", 0) == 0;
}

static std::string extractSection(const std::string &text, const std::string &start,
                                  const std::string &end) {
    const std::size_t begin = text.find(start);
    if (begin == std::string::npos) {
        return "";
    }

    std::size_t bodyStart = begin + start.size();
    while (bodyStart < text.size() &&
           (text[bodyStart] == '\r' || text[bodyStart] == '\n')) {
        ++bodyStart;
    }

    std::size_t finish = text.size();
    if (!end.empty()) {
        const std::size_t foundEnd = text.find(end, bodyStart);
        if (foundEnd != std::string::npos) {
            finish = foundEnd;
        }
    }

    return text.substr(bodyStart, finish - bodyStart);
}

} // namespace

// main
int main(int argc, char *argv[]) {
    // 1. Validasi argumen
    if (argc < 2 || argc > 3) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    const std::string inputFile = argv[1];
    const std::string outputFile = (argc == 3) ? argv[2] : "";

    AstNode ast(AstKind::Program);
    SymbolTable symbols;
    std::vector<SemanticDiagnostic> diagnostics;
    std::string diagnosticsText = "No semantic diagnostics.\n";
    bool semanticOk = true;

    try {
        const std::string inputText = readWholeFile(inputFile);
        if (looksLikeDecoratedAstInput(inputText)) {
            const bool hasReport =
                inputText.find("----- Decorated AST -----") != std::string::npos;
            const std::string astText =
                hasReport
                    ? extractSection(inputText, "----- Decorated AST -----",
                                     "----- Symbol Table -----")
                    : inputText;
            ast = parseRenderedAst(astText);

            if (!hasReport) {
                throw std::runtime_error(
                    "Decorated AST input for Milestone 4 harus menyertakan Symbol Table.");
            }

            const std::string symbolText =
                extractSection(inputText, "----- Symbol Table -----",
                               "----- Semantic Diagnostics -----");
            symbols.loadRenderedAll(symbolText);

            const std::string extractedDiagnostics =
                extractSection(inputText, "----- Semantic Diagnostics -----", "");
            if (!ltrimCopy(extractedDiagnostics).empty()) {
                diagnosticsText = extractedDiagnostics;
            }
            semanticOk = diagnosticsText.find("error:") == std::string::npos;
        } else {
            ParseNode parseTreeNode("<empty>");
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

            ast = buildAstFromParseTree(parseTreeNode);
            SemanticResult semantic = analyzeSemantics(std::move(ast));
            ast = std::move(semantic.ast);
            symbols = std::move(semantic.symbols);
            diagnostics = std::move(semantic.diagnostics);
            diagnosticsText = formatSemanticDiagnostics(diagnostics);
            semanticOk = semantic.ok();
        }
    } catch (const std::runtime_error &e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] Exception tidak terduga: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    bool milestone4Ok = true;
    std::ostringstream output;
    output << "----- Decorated AST -----\n\n";
    output << renderAst(ast);

    output << "\n----- Symbol Table -----\n\n";
    output << symbols.renderAll();

    output << "\n----- Semantic Diagnostics -----\n\n";
    output << diagnosticsText;

    if (semanticOk) {
        try {
            const CodeGeneratorResult generated =
                generateIntermediateCode(ast, symbols);
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

        if (semanticOk) {
            std::cout << "[OK] Analisis selesai. Output ditulis ke '"
                      << outputFile << "'\n";
        } else {
            std::cerr << "[SEMANTIC] Analisis selesai dengan error. Output ditulis ke '"
                      << outputFile << "'\n";
        }
    }

    return semanticOk && milestone4Ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
