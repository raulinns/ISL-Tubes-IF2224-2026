#ifndef INTERMEDIATE_CODE_H
#define INTERMEDIATE_CODE_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

enum class OpCode {
    LIT = 1,
    LOD = 2,
    STO = 3,
    CAL = 4,
    INT = 5,
    JMP = 6,
    JPC = 7,
    OPR = 8,
    RET = 9,
    LDA = 10,
    LDI = 11,
    STI = 12,
    BLD = 13,
    BST = 14,
    ADI = 15,
    IXA = 16
};

enum class OprCode {
    NEG = 1,
    ADD = 2,
    SUB = 3,
    MUL = 4,
    DIV = 5,
    MOD = 6,
    EQL = 7,
    NEQ = 8,
    LSS = 9,
    GEQ = 10,
    GTR = 11,
    LEQ = 12,
    WRT = 13,
    WRTLN = 14,
    IDIV = 15,
    AND = 16,
    OR = 17,
    NOT = 18,
    READLN = 19
};

struct Instruction {
    OpCode op;
    int level;
    int arg;
    std::vector<int> extraArgs;
    std::string literalText;
    std::string comment;

    Instruction(OpCode opcode, int lexicalLevel, int argument,
                std::string note = "");
    Instruction(OpCode opcode, int lexicalLevel, int argument,
                std::string literal, std::string note);
    Instruction(OpCode opcode, int lexicalLevel, int argument,
                std::vector<int> operands, std::string note = "");
};

std::string opcodeToString(OpCode op);
OpCode opcodeFromString(const std::string &name);
std::optional<OprCode> decodeOprCode(int value);
std::string oprCodeToString(OprCode op);
std::string renderInstruction(const Instruction &instruction,
                              std::size_t index);
std::string renderIntermediateCode(const std::vector<Instruction> &code);
std::vector<Instruction> parseIntermediateCode(const std::string &text);

#endif // INTERMEDIATE_CODE_H
