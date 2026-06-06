#include "intermediate_code.h"

#include <cctype>
#include <sstream>
#include <stdexcept>

Instruction::Instruction(OpCode opcode, int lexicalLevel, int argument,
                         std::string note)
    : op(opcode), level(lexicalLevel), arg(argument), comment(std::move(note)) {}

Instruction::Instruction(OpCode opcode, int lexicalLevel, int argument,
                         std::string literal, std::string note)
    : op(opcode), level(lexicalLevel), arg(argument),
      literalText(std::move(literal)), comment(std::move(note)) {}

Instruction::Instruction(OpCode opcode, int lexicalLevel, int argument,
                         std::vector<int> operands, std::string note)
    : op(opcode), level(lexicalLevel), arg(argument),
      extraArgs(std::move(operands)), comment(std::move(note)) {}

std::string opcodeToString(OpCode op) {
    switch (op) {
    case OpCode::LIT:
        return "LIT";
    case OpCode::LOD:
        return "LOD";
    case OpCode::STO:
        return "STO";
    case OpCode::CAL:
        return "CAL";
    case OpCode::INT:
        return "INT";
    case OpCode::JMP:
        return "JMP";
    case OpCode::JPC:
        return "JPC";
    case OpCode::OPR:
        return "OPR";
    case OpCode::RET:
        return "RET";
    case OpCode::LDA:
        return "LDA";
    case OpCode::LDI:
        return "LDI";
    case OpCode::STI:
        return "STI";
    case OpCode::BLD:
        return "BLD";
    case OpCode::BST:
        return "BST";
    case OpCode::ADI:
        return "ADI";
    case OpCode::IXA:
        return "IXA";
    }

    throw std::invalid_argument("Unknown opcode value");
}

OpCode opcodeFromString(const std::string &name) {
    if (name == "LIT") return OpCode::LIT;
    if (name == "LOD") return OpCode::LOD;
    if (name == "STO") return OpCode::STO;
    if (name == "CAL") return OpCode::CAL;
    if (name == "INT") return OpCode::INT;
    if (name == "JMP") return OpCode::JMP;
    if (name == "JPC") return OpCode::JPC;
    if (name == "OPR") return OpCode::OPR;
    if (name == "RET") return OpCode::RET;
    if (name == "LDA") return OpCode::LDA;
    if (name == "LDI") return OpCode::LDI;
    if (name == "STI") return OpCode::STI;
    if (name == "BLD") return OpCode::BLD;
    if (name == "BST") return OpCode::BST;
    if (name == "ADI") return OpCode::ADI;
    if (name == "IXA") return OpCode::IXA;
    throw std::invalid_argument("Unknown opcode: " + name);
}

std::optional<OprCode> decodeOprCode(int value) {
    switch (value) {
    case 1:
        return OprCode::NEG;
    case 2:
        return OprCode::ADD;
    case 3:
        return OprCode::SUB;
    case 4:
        return OprCode::MUL;
    case 5:
        return OprCode::DIV;
    case 6:
        return OprCode::MOD;
    case 7:
        return OprCode::EQL;
    case 8:
        return OprCode::NEQ;
    case 9:
        return OprCode::LSS;
    case 10:
        return OprCode::GEQ;
    case 11:
        return OprCode::GTR;
    case 12:
        return OprCode::LEQ;
    case 13:
        return OprCode::WRT;
    case 14:
        return OprCode::WRTLN;
    case 15:
        return OprCode::IDIV;
    case 16:
        return OprCode::AND;
    case 17:
        return OprCode::OR;
    case 18:
        return OprCode::NOT;
    case 19:
        return OprCode::READLN;
    default:
        return std::nullopt;
    }
}

std::string oprCodeToString(OprCode op) {
    switch (op) {
    case OprCode::NEG:
        return "NEG";
    case OprCode::ADD:
        return "ADD";
    case OprCode::SUB:
        return "SUB";
    case OprCode::MUL:
        return "MUL";
    case OprCode::DIV:
        return "DIV";
    case OprCode::MOD:
        return "MOD";
    case OprCode::EQL:
        return "EQL";
    case OprCode::NEQ:
        return "NEQ";
    case OprCode::LSS:
        return "LSS";
    case OprCode::GEQ:
        return "GEQ";
    case OprCode::GTR:
        return "GTR";
    case OprCode::LEQ:
        return "LEQ";
    case OprCode::WRT:
        return "WRT";
    case OprCode::WRTLN:
        return "WRTLN";
    case OprCode::IDIV:
        return "IDIV";
    case OprCode::AND:
        return "AND";
    case OprCode::OR:
        return "OR";
    case OprCode::NOT:
        return "NOT";
    case OprCode::READLN:
        return "READLN";
    }

    throw std::invalid_argument("Unknown OPR code value");
}

std::string renderInstruction(const Instruction &instruction,
                              std::size_t index) {
    std::ostringstream out;
    out << index << ' ' << opcodeToString(instruction.op) << ' '
        << instruction.level << ' ';
    if (instruction.op == OpCode::LIT && !instruction.literalText.empty()) {
        out << instruction.literalText;
    } else {
        out << instruction.arg;
    }
    for (const int operand : instruction.extraArgs) {
        out << ' ' << operand;
    }

    if (!instruction.comment.empty()) {
        out << " ; " << instruction.comment;
    }

    return out.str();
}

namespace {

std::string trim(const std::string &text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::vector<std::string> tokenizeInstruction(const std::string &line) {
    std::vector<std::string> tokens;
    std::string token;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '\'' && quoted && i + 1 < line.size() && line[i + 1] == '\'') {
            token += "''";
            ++i;
            continue;
        }
        if (ch == '\'') {
            quoted = !quoted;
            token += ch;
            continue;
        }
        if (!quoted && ch == ';') {
            break;
        }
        if (!quoted && std::isspace(static_cast<unsigned char>(ch))) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
            continue;
        }
        token += ch;
    }
    if (quoted) {
        throw std::runtime_error("Unterminated quoted literal in intermediate code");
    }
    if (!token.empty()) {
        tokens.push_back(token);
    }
    return tokens;
}

int parseInteger(const std::string &text, const std::string &context) {
    std::size_t parsed = 0;
    const int value = std::stoi(text, &parsed);
    if (parsed != text.size()) {
        throw std::runtime_error("Invalid integer " + context + ": " + text);
    }
    return value;
}

} // namespace

std::vector<Instruction> parseIntermediateCode(const std::string &text) {
    std::vector<Instruction> code;
    std::istringstream input(text);
    std::string line;
    int sourceLine = 0;
    while (std::getline(input, line)) {
        ++sourceLine;
        if (trim(line).empty()) {
            continue;
        }
        const std::vector<std::string> tokens = tokenizeInstruction(line);
        if (tokens.empty()) {
            continue;
        }
        if (tokens.size() < 4) {
            throw std::runtime_error("Incomplete instruction at line " +
                                     std::to_string(sourceLine));
        }
        const int index = parseInteger(tokens[0], "instruction index");
        if (index != static_cast<int>(code.size())) {
            throw std::runtime_error("Non-contiguous instruction index at line " +
                                     std::to_string(sourceLine));
        }
        const OpCode op = opcodeFromString(tokens[1]);
        const int level = parseInteger(tokens[2], "lexical level");
        Instruction instruction(op, level, 0);
        if (op == OpCode::LIT) {
            instruction.literalText = tokens[3];
            try {
                instruction.arg = parseInteger(tokens[3], "literal");
            } catch (const std::exception &) {
                instruction.arg = 0;
            }
        } else {
            instruction.arg = parseInteger(tokens[3], "operand");
        }
        for (std::size_t i = 4; i < tokens.size(); ++i) {
            instruction.extraArgs.push_back(
                parseInteger(tokens[i], "additional operand"));
        }
        code.push_back(std::move(instruction));
    }
    return code;
}

std::string renderIntermediateCode(const std::vector<Instruction> &code) {
    std::ostringstream out;
    for (std::size_t i = 0; i < code.size(); ++i) {
        out << renderInstruction(code[i], i) << '\n';
    }
    return out.str();
}
