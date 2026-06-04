#include "intermediate_code.h"

#include <sstream>
#include <stdexcept>

Instruction::Instruction(OpCode opcode, int lexicalLevel, int argument,
                         std::string note)
    : op(opcode), level(lexicalLevel), arg(argument), comment(std::move(note)) {}

Instruction::Instruction(OpCode opcode, int lexicalLevel, int argument,
                         std::string literal, std::string note)
    : op(opcode), level(lexicalLevel), arg(argument),
      literalText(std::move(literal)), comment(std::move(note)) {}

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
    }

    throw std::invalid_argument("Unknown opcode value");
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

    if (!instruction.comment.empty()) {
        out << " ; " << instruction.comment;
    }

    return out.str();
}

std::string renderIntermediateCode(const std::vector<Instruction> &code) {
    std::ostringstream out;
    for (std::size_t i = 0; i < code.size(); ++i) {
        out << renderInstruction(code[i], i) << '\n';
    }
    return out.str();
}
