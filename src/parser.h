#ifndef PARSER_H
#define PARSER_H

#include "token.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

struct ParseNode {
  std::string label;
  std::vector<ParseNode> children;

  explicit ParseNode(std::string text) : label(std::move(text)) {}
};

class Parser {
  public:
    explicit Parser(std::vector<Token> tokens);

    ParseNode parseProgram();

  private:
    std::vector<Token> tokens_;
    std::size_t pos_;

    const Token &current() const;
    const Token &peek(std::size_t offset = 0) const;
    bool isAtEnd() const;
    bool check(TokenType type) const;
    bool match(TokenType type);

    ParseNode parseProgramHeader();
    ParseNode parseDeclarationPart();
    ParseNode parseConstDeclaration();
    ParseNode parseConstant();
    ParseNode parseTypeDeclaration();
    ParseNode parseVarDeclaration();
    ParseNode parseIdentifierList();
    ParseNode parseType();
    ParseNode parseArrayType();
    ParseNode parseRange();
    ParseNode parseEnumerated();
    ParseNode parseRecordType();
    ParseNode parseFieldList();
    ParseNode parseFieldPart();
    ParseNode parseSubprogramDeclaration();
    ParseNode parseProcedureDeclaration();
    ParseNode parseFunctionDeclaration();
    ParseNode parseBlock();
    ParseNode parseFormalParameterList();
    ParseNode parseParameterGroup();
    ParseNode parseCompoundStatement();
    ParseNode parseStatementList(TokenType terminator);
    ParseNode parseStatement();
    ParseNode parseAssignmentStatement();
    ParseNode parseIfStatement();
    ParseNode parseCaseStatement();
    ParseNode parseCaseBlock();
    ParseNode parseWhileStatement();
    ParseNode parseRepeatStatement();
    ParseNode parseForStatement();
    ParseNode parseProcedureOrFunctionCall();
    ParseNode parseParameterList();
    ParseNode parseExpression();
    ParseNode parseSimpleExpression();
    ParseNode parseTerm();
    ParseNode parseFactor();
    ParseNode parseRelationalOperator();
    ParseNode parseAdditiveOperator();
    ParseNode parseMultiplicativeOperator();

    ParseNode consume(TokenType expected);
    [[noreturn]] void syntaxError(const std::string &message) const;

    bool canStartStatement() const;
    bool isStatementFollowToken(TokenType type) const;
    bool looksLikeRange() const;
    bool looksLikeProcedureOrFunctionCall() const;
    static bool canStartConstant(TokenType type);
};

#endif
