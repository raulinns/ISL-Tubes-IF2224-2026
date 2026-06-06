#include "../../src/milestone4/arion_runtime_error.h"
#include "../../src/milestone4/intermediate_code.h"
#include "../../src/milestone4/interpreter.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

Instruction withExtra(OpCode op, int level, int arg,
                      std::vector<int> extra) {
    return Instruction(op, level, arg, std::move(extra));
}

void expectFailure(const std::string &name,
                   const std::vector<Instruction> &code,
                   const std::string &message,
                   std::size_t maxFrames = 1000) {
    try {
        runIntermediateCode(code, maxFrames);
    } catch (const ArionRuntimeError &error) {
        if (std::string(error.what()).find(message) == std::string::npos ||
            std::string(error.what()).find("ip ") == std::string::npos) {
            throw std::runtime_error(name + " returned unexpected error: " +
                                     error.what());
        }
        return;
    }
    throw std::runtime_error(name + " did not fail");
}

void testRoundTrip() {
    const std::vector<Instruction> original = {
        Instruction(OpCode::INT, 0, 5),
        Instruction(OpCode::LIT, 0, 0, "'hello world'", ""),
        withExtra(OpCode::CAL, 1, 4, {2}),
        withExtra(OpCode::IXA, 0, -2, {3, 4}),
        Instruction(OpCode::RET, 0, 0)};
    const std::string rendered = renderIntermediateCode(original);
    const std::vector<Instruction> parsed = parseIntermediateCode(rendered);
    if (renderIntermediateCode(parsed) != rendered) {
        throw std::runtime_error("Intermediate-code round-trip changed output");
    }
}

} // namespace

int main() {
    try {
        testRoundTrip();
        expectFailure(
            "bounds",
            {Instruction(OpCode::INT, 0, 4),
             Instruction(OpCode::LDA, 0, 3),
             Instruction(OpCode::LIT, 0, 4),
             withExtra(OpCode::IXA, 0, 1, {3, 1}),
             Instruction(OpCode::RET, 0, 0)},
            "outside");
        expectFailure("invalid jump",
                      {Instruction(OpCode::JMP, 0, 99)},
                      "invalid jump/call target");
        expectFailure(
            "division zero",
            {Instruction(OpCode::INT, 0, 3),
             Instruction(OpCode::LIT, 0, 1),
             Instruction(OpCode::LIT, 0, 0),
             Instruction(OpCode::OPR, 0,
                         static_cast<int>(OprCode::IDIV)),
             Instruction(OpCode::RET, 0, 0)},
            "zero");
        expectFailure(
            "integer overflow",
            {Instruction(OpCode::INT, 0, 3),
             Instruction(OpCode::LIT, 0, 2147483647),
             Instruction(OpCode::LIT, 0, 1),
             Instruction(OpCode::OPR, 0,
                         static_cast<int>(OprCode::ADD)),
             Instruction(OpCode::RET, 0, 0)},
            "overflow");
        expectFailure(
            "real underflow",
            {Instruction(OpCode::INT, 0, 3),
             Instruction(OpCode::LIT, 0, 0, "1.0e-300", ""),
             Instruction(OpCode::LIT, 0, 0, "1.0e-300", ""),
             Instruction(OpCode::OPR, 0,
                         static_cast<int>(OprCode::MUL)),
             Instruction(OpCode::RET, 0, 0)},
            "underflow");
        expectFailure(
            "operand underflow",
            {Instruction(OpCode::INT, 0, 3),
             Instruction(OpCode::OPR, 0,
                         static_cast<int>(OprCode::ADD)),
             Instruction(OpCode::RET, 0, 0)},
            "requires more operands");
        expectFailure("frame underflow",
                      {Instruction(OpCode::LOD, 0, 3)},
                      "No activation frame");
        expectFailure(
            "return corruption",
            {Instruction(OpCode::INT, 0, 3),
             Instruction(OpCode::LIT, 0, 1),
             Instruction(OpCode::RET, 0, 0)},
            "Invalid global RET state");
        expectFailure(
            "header protection",
            {Instruction(OpCode::INT, 0, 3),
             Instruction(OpCode::LOD, 0, 2),
             Instruction(OpCode::RET, 0, 0)},
            "outside writable frame slots");
        expectFailure(
            "argument contract",
            {Instruction(OpCode::INT, 0, 3),
             withExtra(OpCode::CAL, 0, 3, {1}),
             Instruction(OpCode::RET, 0, 0),
             Instruction(OpCode::INT, 0, 3),
             Instruction(OpCode::RET, 0, 0)},
            "argument-slot count");
        expectFailure(
            "frame overflow",
            {Instruction(OpCode::INT, 0, 3),
             withExtra(OpCode::CAL, 0, 3, {0}),
             Instruction(OpCode::RET, 0, 0),
             Instruction(OpCode::INT, 0, 3),
             withExtra(OpCode::CAL, 1, 3, {0}),
             Instruction(OpCode::RET, 0, 0)},
            "frame limit", 4);
        std::cout << "runtime tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
