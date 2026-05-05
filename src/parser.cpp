#include "parser.h"

#include <stdexcept>
#include <utility>

namespace {

const Token kEofToken(TOKEN_ERROR, "<EOF>", -1);

ParseNode makeTokenNode(const Token &token) {
  std::string label = tokenTypeToString(token.type);
  if (tokenNeedsValue(token.type) && !token.value.empty()) {
    label += " (" + token.value + ")";
  }
  return ParseNode(label);
}

} // namespace

Parser::Parser(std::vector<Token> tokens) : pos_(0) {
  tokens_.reserve(tokens.size());
  for (const Token &token : tokens) {
    if (token.type != COMMENT) {
      tokens_.push_back(token);
    }
  }
}

ParseNode Parser::parseProgram() { notImplemented("<program>"); }

const Token &Parser::current() const { return peek(); }

const Token &Parser::peek(std::size_t offset) const {
  if (pos_ + offset >= tokens_.size()) {
    return kEofToken;
  }
  return tokens_[pos_ + offset];
}

bool Parser::isAtEnd() const { return pos_ >= tokens_.size(); }

bool Parser::check(TokenType type) const {
  return !isAtEnd() && current().type == type;
}

bool Parser::match(TokenType type) {
  if (!check(type)) {
    return false;
  }
  advance();
  return true;
}

void Parser::advance() {
  if (!isAtEnd()) {
    ++pos_;
  }
}

ParseNode Parser::consume(TokenType expected) {
  if (!check(expected)) {
    syntaxError("unexpected token " + tokenTypeToString(current().type) +
                ", expected " + tokenTypeToString(expected));
  }

  ParseNode node = makeTokenNode(current());
  advance();
  return node;
}

[[noreturn]] void Parser::syntaxError(const std::string &message) const {
  const Token &token = current();
  if (token.line > 0) {
    throw std::runtime_error("Syntax error at line " +
                             std::to_string(token.line) + ": " + message);
  }
  throw std::runtime_error("Syntax error at end of input: " + message);
}

[[noreturn]] void Parser::notImplemented(const std::string &ruleName) const {
  throw std::runtime_error("Parser rule not implemented yet: " + ruleName);
}

bool Parser::canStartStatement() const {
  return check(IDENT) || check(IFSY) || check(CASESY) || check(WHILESY) ||
         check(REPEATSY) || check(FORSY) || check(BEGINSY);
}

bool Parser::isStatementFollowToken(TokenType type) const {
  return type == SEMICOLON || type == ENDSY || type == ELSESY ||
         type == UNTILSY || type == PERIOD;
}

bool Parser::looksLikeRange() const {
  std::size_t index = pos_;
  if (index >= tokens_.size()) {
    return false;
  }

  if (tokens_[index].type == PLUS || tokens_[index].type == MINUS) {
    ++index;
  }

  if (index >= tokens_.size()) {
    return false;
  }

  const TokenType baseType = tokens_[index].type;
  if (baseType != IDENT && baseType != INTCON && baseType != REALCON &&
      baseType != CHARCON && baseType != STRING) {
    return false;
  }

  return index + 2 < tokens_.size() && tokens_[index + 1].type == PERIOD &&
         tokens_[index + 2].type == PERIOD;
}

bool Parser::looksLikeProcedureOrFunctionCall() const {
  return check(IDENT) && peek(1).type == LPARENT;
}

bool Parser::canStartConstant(TokenType type) {
  return type == PLUS || type == MINUS || type == IDENT || type == INTCON ||
         type == REALCON || type == CHARCON || type == STRING;
}

ParseNode Parser::parseProgramHeader() { notImplemented("<program-header>"); }

ParseNode Parser::parseDeclarationPart() {
  notImplemented("<declaration-part>");
}

ParseNode Parser::parseConstDeclaration() {
  notImplemented("<const-declaration>");
}

ParseNode Parser::parseConstant() { notImplemented("<constant>"); }

ParseNode Parser::parseTypeDeclaration() { notImplemented("<type-declaration>"); }

ParseNode Parser::parseVarDeclaration() { notImplemented("<var-declaration>"); }

ParseNode Parser::parseIdentifierList() { notImplemented("<identifier-list>"); }

ParseNode Parser::parseType() { notImplemented("<type>"); }

ParseNode Parser::parseArrayType() { notImplemented("<array-type>"); }

ParseNode Parser::parseRange() { notImplemented("<range>"); }

ParseNode Parser::parseEnumerated() { notImplemented("<enumerated>"); }

ParseNode Parser::parseRecordType() { notImplemented("<record-type>"); }

ParseNode Parser::parseFieldList() { notImplemented("<field-list>"); }

ParseNode Parser::parseFieldPart() { notImplemented("<field-part>"); }

ParseNode Parser::parseSubprogramDeclaration() {
  notImplemented("<subprogram-declaration>");
}

ParseNode Parser::parseProcedureDeclaration() {
  notImplemented("<procedure-declaration>");
}

ParseNode Parser::parseFunctionDeclaration() {
  notImplemented("<function-declaration>");
}

ParseNode Parser::parseBlock() { notImplemented("block"); }

ParseNode Parser::parseFormalParameterList() {
  notImplemented("<formal-parameter-list>");
}

ParseNode Parser::parseParameterGroup() {
  notImplemented("<parameter-group>");
}

ParseNode Parser::parseCompoundStatement() {
  notImplemented("<compound-statement>");
}

ParseNode Parser::parseStatementList(TokenType) {
  notImplemented("<statement-list>");
}

ParseNode Parser::parseStatement() { notImplemented("<statement>"); }

ParseNode Parser::parseAssignmentStatement() {
  notImplemented("<assignment-statement>");
}

ParseNode Parser::parseIfStatement() { notImplemented("<if-statement>"); }

ParseNode Parser::parseCaseStatement() { notImplemented("<case-statement>"); }

ParseNode Parser::parseCaseBlock() { notImplemented("<case-block>"); }

ParseNode Parser::parseWhileStatement() {
  notImplemented("<while-statement>");
}

ParseNode Parser::parseRepeatStatement() {
  notImplemented("<repeat-statement>");
}

ParseNode Parser::parseForStatement() { notImplemented("<for-statement>"); }

ParseNode Parser::parseProcedureOrFunctionCall() {
  notImplemented("<procedure/function-call>");
}

ParseNode Parser::parseParameterList() { notImplemented("<parameter-list>"); }

ParseNode Parser::parseExpression() { notImplemented("<expression>"); }

ParseNode Parser::parseSimpleExpression() {
  notImplemented("<simple-expression>");
}

ParseNode Parser::parseTerm() { notImplemented("<term>"); }

ParseNode Parser::parseFactor() { notImplemented("<factor>"); }

ParseNode Parser::parseRelationalOperator() {
  notImplemented("<relational-operator>");
}

ParseNode Parser::parseAdditiveOperator() {
  notImplemented("<additive-operator>");
}

ParseNode Parser::parseMultiplicativeOperator() {
  notImplemented("<multiplicative-operator>");
}
