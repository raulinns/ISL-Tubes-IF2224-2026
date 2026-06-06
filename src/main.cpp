#include "ast_builder.h"
#include "frontend_artifact.h"
#include "lexer.h"
#include "milestone4/code_generator.h"
#include "milestone4/intermediate_code.h"
#include "milestone4/interpreter.h"
#include "parse_tree.h"
#include "parser.h"
#include "semantic_analyzer.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

enum class InputMode { Artifact, Source, ParseTree };

struct CliOptions {
    InputMode mode = InputMode::Artifact;
    std::string inputPath;
    std::string outputPath;
};

void printUsage(const char *program) {
    std::cerr << "Penggunaan:\n"
              << "  " << program << " input.txt [output.txt]\n"
              << "  " << program << " --source input.txt [output.txt]\n"
              << "  " << program << " --parse-tree input.txt [output.txt]\n";
}

CliOptions parseArguments(int argc, char **argv) {
    CliOptions options;
    int index = 1;
    if (index < argc && std::string(argv[index]) == "--source") {
        options.mode = InputMode::Source;
        ++index;
    } else if (index < argc && std::string(argv[index]) == "--parse-tree") {
        options.mode = InputMode::ParseTree;
        ++index;
    }
    const int remaining = argc - index;
    if (remaining < 1 || remaining > 2) {
        throw std::invalid_argument("invalid command-line arguments");
    }
    options.inputPath = argv[index];
    if (remaining == 2) {
        options.outputPath = argv[index + 1];
    }
    return options;
}

std::string readWholeFile(const std::string &path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Tidak dapat membuka file: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool hasLexicalError(const std::vector<Token> &tokens) {
    bool hasError = false;
    for (const Token &token : tokens) {
        if (token.type == TOKEN_ERROR) {
            std::cerr << "[LEXER] Baris " << token.line << ": token tidak valid '"
                      << token.value << "'\n";
            hasError = true;
        }
    }
    return hasError;
}

FrontendArtifact analyzeAst(AstNode ast) {
    SemanticResult semantic = analyzeSemantics(std::move(ast));
    const bool semanticOk = semantic.ok();
    FrontendArtifact artifact;
    artifact.ast = std::move(semantic.ast);
    artifact.symbols = std::move(semantic.symbols);
    artifact.diagnostics = std::move(semantic.diagnostics);
    if (semanticOk != artifact.semanticOk()) {
        throw std::logic_error("Semantic status changed while building artifact");
    }
    return artifact;
}

FrontendArtifact loadFrontend(const CliOptions &options) {
    if (options.mode == InputMode::Artifact) {
        return parseFrontendArtifact(readWholeFile(options.inputPath));
    }
    if (options.mode == InputMode::ParseTree) {
        return analyzeAst(
            buildAstFromParseTree(parseRenderedParseTree(
                readWholeFile(options.inputPath))));
    }

    Lexer lexer(options.inputPath);
    std::vector<Token> tokens = lexer.tokenize();
    if (hasLexicalError(tokens)) {
        throw std::runtime_error(
            "Parsing dibatalkan karena terdapat lexical error");
    }
    Parser parser(tokens);
    return analyzeAst(buildAstFromParseTree(parser.parseProgram()));
}

void writeOutput(const std::string &path, const std::string &text) {
    if (path.empty()) {
        std::cout << text;
        return;
    }
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Tidak dapat membuat file output: " + path);
    }
    output << text;
}

} // namespace

int main(int argc, char **argv) {
    CliOptions options;
    try {
        options = parseArguments(argc, argv);
    } catch (const std::exception &) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    try {
        FrontendArtifact artifact = loadFrontend(options);
        validateFrontendArtifact(artifact);
        normalizeRuntimeLayout(artifact.ast, artifact.symbols);

        std::ostringstream report;
        report << renderFrontendArtifact(artifact);
        if (!artifact.semanticOk()) {
            writeOutput(options.outputPath, report.str());
            std::cerr << "[SEMANTIC] Code generation dibatalkan karena semantic "
                         "error.\n";
            return EXIT_FAILURE;
        }

        const CodeGeneratorResult generated =
            generateIntermediateCode(artifact.ast, artifact.symbols);
        const std::string renderedCode = renderIntermediateCode(generated.code);
        const std::vector<Instruction> parsedCode =
            parseIntermediateCode(renderedCode);
        report << "\n----- Intermediate Code -----\n\n" << renderedCode;

        const InterpreterResult result = runIntermediateCode(parsedCode);
        report << "\n----- Program Output -----\n\n" << result.output;
        writeOutput(options.outputPath, report.str());
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        std::cerr << "[ERROR] " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
