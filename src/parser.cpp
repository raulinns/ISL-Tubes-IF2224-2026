#include "parser.h"

#include <stdexcept>
#include <utility>

namespace {

const Token kEofToken(TOKEN_ERROR, "<EOF>", -1);

ParseNode makeTokenNode(const Token &token) {
    std::string label = tokenTypeToString(token.type);
    if (tokenNeedsValue(token.type) && !token.value.empty()) {
        label += "(" + token.value + ")";
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

ParseNode Parser::parseProgram() {
    ParseNode node("<program>");

    node.addChild(parseProgramHeader());
    node.addChild(parseDeclarationPart());
    node.addChild(parseCompoundStatement());
    node.addChild(consume(PERIOD));

    return node;
}

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
           type == REALCON || type == CHARCON || type == STRING ||
           type == BOOLCON;
}

ParseNode Parser::parseProgramHeader() {
    ParseNode node("<program-header>");

    node.addChild(consume(PROGRAMSY));
    node.addChild(consume(IDENT));
    node.addChild(consume(SEMICOLON));

    return node;
}

ParseNode Parser::parseDeclarationPart() {
    ParseNode node("<declaration-part>");

    while (check(CONSTSY)) {
        node.addChild(parseConstDeclaration());
    }

    while (check(TYPESY)) {
        node.addChild(parseTypeDeclaration());
    }

    if (check(CONSTSY)) {
        syntaxError("const declarations must appear in a single section before "
                    "type, var, and subprogram declarations");
    }

    while (check(VARSY)) {
        node.addChild(parseVarDeclaration());
    }

    if (check(CONSTSY) || check(TYPESY)) {
        syntaxError("declarations must be ordered as const, type, var, then "
                    "subprogram");
    }

    while (check(PROCEDURESY) || check(FUNCTIONSY)) {
        node.addChild(parseSubprogramDeclaration());
    }

    if (check(CONSTSY) || check(TYPESY) || check(VARSY)) {
        syntaxError("declarations must be ordered as const, type, var, then "
                    "subprogram");
    }

    return node;
}

ParseNode Parser::parseConstDeclaration() {
    ParseNode node("<const-declaration>");
    node.addChild(consume(CONSTSY));

    do {
        node.addChild(consume(IDENT));
        node.addChild(consume(EQL)); // Menggunakan '==' sesuai spesifikasi
        node.addChild(parseConstant());
        node.addChild(consume(SEMICOLON));
    } while (check(IDENT)); // Selama ada identifier baru setelah semicolon

    return node;
}

ParseNode Parser::parseConstant() {
    ParseNode node("<constant>");

    if (check(CHARCON)) {
        node.addChild(consume(CHARCON));
    } else if (check(STRING)) {
        node.addChild(consume(STRING));
    } else {
        if (check(PLUS) || check(MINUS)) {
            node.addChild(consume(current().type));
        }

        if (check(IDENT)) {
            node.addChild(consume(IDENT));
        } else if (check(BOOLCON)) {
            node.addChild(consume(BOOLCON));
        } else if (check(INTCON)) {
            node.addChild(consume(INTCON));
        } else if (check(REALCON)) {
            node.addChild(consume(REALCON));
        } else {
            syntaxError("Expected constant identifier, integer, or real");
        }
    }
    return node;
}

ParseNode Parser::parseTypeDeclaration() {
    ParseNode node("<type-declaration>");
    node.addChild(consume(TYPESY));

    do {
        node.addChild(consume(IDENT));
        node.addChild(consume(EQL));
        node.addChild(parseType());
        node.addChild(consume(SEMICOLON));
    } while (check(IDENT));

    return node;
}

ParseNode Parser::parseVarDeclaration() {
    ParseNode node("<var-declaration>");
    node.addChild(consume(VARSY));

    do {
        node.addChild(parseIdentifierList());
        node.addChild(consume(COLON));
        node.addChild(parseType());
        node.addChild(consume(SEMICOLON));
    } while (check(IDENT));

    return node;
}

ParseNode Parser::parseIdentifierList() {
    ParseNode node("<identifier-list>");
    node.addChild(consume(IDENT));

    while (match(COMMA)) {
        node.addChild(makeTokenNode(Token(COMMA)));
        node.addChild(consume(IDENT));
    }
    return node;
}

ParseNode Parser::parseType() {
    ParseNode node("<type>");

    if (check(ARRAYSY)) {
        node.addChild(parseArrayType());
    } else if (check(LPARENT)) {
        node.addChild(parseEnumerated());
    } else if (check(RECORDSY)) {
        node.addChild(parseRecordType());
    } else if (looksLikeRange()) {
        node.addChild(parseRange());
    } else if (check(IDENT)) {
        node.addChild(consume(IDENT));
    } else {
        syntaxError("Unknown type definition");
    }

    return node;
}

ParseNode Parser::parseArrayType() {
    ParseNode node("<array-type>");
    node.addChild(consume(ARRAYSY));
    node.addChild(consume(LBRACK));

    if (looksLikeRange()) {
        node.addChild(parseRange());
    } else {
        node.addChild(consume(IDENT));
    }

    node.addChild(consume(RBRACK));
    node.addChild(consume(OFSY));
    node.addChild(parseType());

    return node;
}

ParseNode Parser::parseRange() {
    ParseNode node("<range>");
    node.addChild(parseConstant());
    node.addChild(consume(PERIOD));
    node.addChild(consume(PERIOD));
    node.addChild(parseConstant());
    return node;
}

ParseNode Parser::parseEnumerated() {
    ParseNode node("<enumerated>");
    node.addChild(consume(LPARENT));
    node.addChild(consume(IDENT));
    while (match(COMMA)) {
        node.addChild(makeTokenNode(Token(COMMA)));
        node.addChild(consume(IDENT));
    }
    node.addChild(consume(RPARENT));
    return node;
}

ParseNode Parser::parseRecordType() {
    ParseNode node("<record-type>");

    node.addChild(consume(RECORDSY));
    node.addChild(parseFieldList());
    node.addChild(consume(ENDSY));

    return node;
}

ParseNode Parser::parseFieldList() {
    ParseNode node("<field-list>");

    if (check(ENDSY)) {
        return node;
    }

    node.addChild(parseFieldPart());

    while (check(SEMICOLON)) {
        node.addChild(consume(SEMICOLON));
        if (check(ENDSY)) {
            break;
        }
        node.addChild(parseFieldPart());
    }

    return node;
}

ParseNode Parser::parseFieldPart() {
    ParseNode node("<field-part>");

    node.addChild(parseIdentifierList());
    node.addChild(consume(COLON));
    node.addChild(parseType());

    return node;
}

ParseNode Parser::parseSubprogramDeclaration() {
    ParseNode node("<subprogram-declaration>");

    if (check(PROCEDURESY)) {
        node.addChild(parseProcedureDeclaration());
    } else if (check(FUNCTIONSY)) {
        node.addChild(parseFunctionDeclaration());
    } else {
        syntaxError(
            "expected 'procedure' or 'function' in subprogram declaration");
    }

    return node;
}

ParseNode Parser::parseProcedureDeclaration() {
    ParseNode node("<procedure-declaration>");

    // proceduresy
    node.addChild(consume(PROCEDURESY));

    // ident (nama prosedur)
    node.addChild(consume(IDENT));

    // (formal-parameter-list)?. opsional, kalo ada lparent
    if (check(LPARENT)) {
        node.addChild(parseFormalParameterList());
    }

    // semicolon
    node.addChild(consume(SEMICOLON));

    // block
    node.addChild(parseBlock());

    // semicolon
    node.addChild(consume(SEMICOLON));

    return node;
}

ParseNode Parser::parseFunctionDeclaration() {
    ParseNode node("<function-declaration>");

    // functionsy
    node.addChild(consume(FUNCTIONSY));

    // ident (nama fungsi)
    node.addChild(consume(IDENT));

    // (formal-parameter-list)?
    if (check(LPARENT)) {
        node.addChild(parseFormalParameterList());
    }

    // colon
    node.addChild(consume(COLON));

    // ident (tipe return)
    node.addChild(consume(IDENT));

    // semicolon
    node.addChild(consume(SEMICOLON));

    // block
    node.addChild(parseBlock());

    // semicolon
    node.addChild(consume(SEMICOLON));

    return node;
}

ParseNode Parser::parseBlock() {
    ParseNode node("<block>");

    // declaration-part
    node.addChild(parseDeclarationPart());

    // compound-statement
    node.addChild(parseCompoundStatement());

    return node;
}

ParseNode Parser::parseFormalParameterList() {
    ParseNode node("<formal-parameter-list>");

    // lparent
    node.addChild(consume(LPARENT));

    // parameter-group pertama (wajib ada minimal satu)
    node.addChild(parseParameterGroup());

    // (semicolon + parameter-group)*
    while (check(SEMICOLON)) {
        node.addChild(consume(SEMICOLON));
        node.addChild(parseParameterGroup());
    }

    // rparent
    node.addChild(consume(RPARENT));

    return node;
}

ParseNode Parser::parseParameterGroup() {
    ParseNode node("<parameter-group>");

    // identifierlist (punya endra tar)
    node.addChild(parseIdentifierList());

    // colon
    node.addChild(consume(COLON));

    // tipe: ident atau array-type
    if (check(ARRAYSY)) {
        // array-type
        node.addChild(parseArrayType());
    } else {
        // ident (nama tipe sederhana)
        node.addChild(consume(IDENT));
    }

    return node;
}

ParseNode Parser::parseCompoundStatement() {
    ParseNode node("<compound-statement>");

    node.addChild(consume(BEGINSY));
    node.addChild(parseStatementList(ENDSY));
    node.addChild(consume(ENDSY));

    return node;
}

ParseNode Parser::parseStatementList(TokenType terminator) {
    ParseNode node("<statement-list>");

    while (!check(terminator)) {
        node.addChild(parseStatement());

        if (!check(SEMICOLON)) {
            break;
        }

        node.addChild(consume(SEMICOLON));
    }

    return node;
}

ParseNode Parser::parseStatement() {
    ParseNode node("<statement>");

    if (isStatementFollowToken(current().type)) {
        return node;
    }

    if (check(IDENT)) {
        std::size_t index = 1;
        while (true) {
            if (peek(index).type == LBRACK) {
                int depth = 1;
                ++index;
                while (depth > 0 && peek(index).type != TOKEN_ERROR) {
                    if (peek(index).type == LBRACK) {
                        ++depth;
                    } else if (peek(index).type == RBRACK) {
                        --depth;
                    }
                    ++index;
                }
            } else if (peek(index).type == PERIOD &&
                       peek(index + 1).type == IDENT) {
                index += 2;
            } else {
                break;
            }
        }

        if (peek(index).type == BECOMES) {
            node.addChild(parseAssignmentStatement());
        } else if (peek(index).type == LPARENT) {
            node.addChild(parseProcedureOrFunctionCall());
        } else{
            node.addChild(consume(IDENT));
        }
    } else if (check(BEGINSY)) {
        node.addChild(parseCompoundStatement());
    } else if (check(IFSY)) {
        node.addChild(parseIfStatement());
    } else if (check(CASESY)) {
        node.addChild(parseCaseStatement());
    } else if (check(WHILESY)) {
        node.addChild(parseWhileStatement());
    } else if (check(REPEATSY)) {
        node.addChild(parseRepeatStatement());
    } else if (check(FORSY)) {
        node.addChild(parseForStatement());
    } else {
        syntaxError("expected statement");
    }

    return node;
}

ParseNode Parser::parseAssignmentStatement() {
    ParseNode node("<assignment-statement>");

    node.addChild(parseVariable());
    node.addChild(consume(BECOMES));
    node.addChild(parseExpression());

    return node;
}

ParseNode Parser::parseVariable() {
    ParseNode node("<variable>");

    node.addChild(consume(IDENT));

    while (check(LBRACK) || check(PERIOD)) {
        ParseNode component("<component-variable>");

        if (check(LBRACK)) {
            component.addChild(consume(LBRACK));

            ParseNode indexList("<index-list>");
            indexList.addChild(parseExpression());

            while (check(COMMA)) {
                indexList.addChild(consume(COMMA));
                indexList.addChild(parseExpression());
            }

            component.addChild(indexList);
            component.addChild(consume(RBRACK));
        } else {
            component.addChild(consume(PERIOD));
            component.addChild(consume(IDENT));
        }

        node.addChild(component);
    }

    return node;
}

ParseNode Parser::parseIfStatement() {
    ParseNode node("<if-statement>");

    // ifsy
    node.addChild(consume(IFSY));

    // expression
    node.addChild(parseExpression());

    // thensy
    node.addChild(consume(THENSY));

    // statement
    node.addChild(parseStatement());

    // (elsesy + statement)?
    if (check(ELSESY)) {
        node.addChild(consume(ELSESY));
        node.addChild(parseStatement());
    }

    return node;
}

ParseNode Parser::parseCaseStatement() {
    ParseNode node("<case-statement>");

    // casesy
    node.addChild(consume(CASESY));

    // expression
    node.addChild(parseExpression());

    // ofsy
    node.addChild(consume(OFSY));

    // case-block
    node.addChild(parseCaseBlock());

    // endsy
    node.addChild(consume(ENDSY));

    return node;
}

ParseNode Parser::parseCaseBlock() {
    ParseNode node("<case-block>");

    // constant pertama
    node.addChild(parseConstant());

    // (comma + constant)*
    while (check(COMMA)) {
        node.addChild(consume(COMMA));
        node.addChild(parseConstant());
    }

    // colon
    node.addChild(consume(COLON));

    // statement
    node.addChild(parseStatement());

    // (semicolon + case-block?)*
    while (check(SEMICOLON)) {
        node.addChild(consume(SEMICOLON));

        // case-block?, lanjut kalo ada constant nextnya
        // (kalo ga ada, berarti ini semicolon trailing sebelum endsy)
        if (!check(ENDSY) && canStartConstant(current().type)) {
            node.addChild(parseCaseBlock());
        }
    }

    return node;
}

ParseNode Parser::parseWhileStatement() {
    ParseNode node("<while-statement>");

    // whilesy
    node.addChild(consume(WHILESY));

    // expression
    node.addChild(parseExpression());

    // dosy
    node.addChild(consume(DOSY));

    // compound-statement
    node.addChild(parseCompoundStatement());

    return node;
}

ParseNode Parser::parseRepeatStatement() {
    ParseNode node("<repeat-statement>");

    // repeatsy
    node.addChild(consume(REPEATSY));

    // statement-list
    node.addChild(parseStatementList(UNTILSY));

    // untilsy
    node.addChild(consume(UNTILSY));

    // expression
    node.addChild(parseExpression());

    return node;
}

ParseNode Parser::parseForStatement() {
    ParseNode node("<for-statement>");

    // forsy
    node.addChild(consume(FORSY));

    // ident (variabel counter)
    node.addChild(consume(IDENT));

    // becomes (:=)
    node.addChild(consume(BECOMES));

    // expression, nilai awal
    node.addChild(parseExpression());

    // tosy atau downtosy
    if (check(TOSY)) {
        node.addChild(consume(TOSY));
    } else if (check(DOWNTOSY)) {
        node.addChild(consume(DOWNTOSY));
    } else {
        syntaxError("expected 'to' or 'downto' in for statement");
    }

    // expression, nilai akhir
    node.addChild(parseExpression());

    // dosy
    node.addChild(consume(DOSY));

    // compound-statement
    node.addChild(parseCompoundStatement());

    return node;
}

ParseNode Parser::parseProcedureOrFunctionCall() {
    ParseNode node("<procedure/function-call>");

    node.addChild(consume(IDENT));

    if (check(LPARENT)) {
        node.addChild(consume(LPARENT));

        if (!check(RPARENT)) {
            node.addChild(parseParameterList());
        }

        node.addChild(consume(RPARENT));
    }

    return node;
}

ParseNode Parser::parseParameterList() {
    ParseNode node("<parameter-list>");

    node.addChild(parseExpression());

    while (check(COMMA)) {
        node.addChild(consume(COMMA));
        node.addChild(parseExpression());
    }

    return node;
}

ParseNode Parser::parseExpression() {
    ParseNode node("<expression>");

    node.addChild(parseSimpleExpression());

    if (check(EQL) || check(NEQ) || check(GTR) || check(GEQ) || check(LSS) ||
        check(LEQ)) {
        node.addChild(parseRelationalOperator());
        node.addChild(parseSimpleExpression());
    }

    return node;
}

ParseNode Parser::parseSimpleExpression() {
    ParseNode node("<simple-expression>");

    if (check(PLUS) || check(MINUS)) {
        node.addChild(consume(current().type));
    }

    node.addChild(parseTerm());

    while (check(PLUS) || check(MINUS) || check(ORSY)) {
        node.addChild(parseAdditiveOperator());
        node.addChild(parseTerm());
    }

    return node;
}

ParseNode Parser::parseTerm() {
    ParseNode node("<term>");

    node.addChild(parseFactor());

    while (check(TIMES) || check(IDIV) || check(RDIV) || check(IMOD) ||
           check(ANDSY)) {
        node.addChild(parseMultiplicativeOperator());
        node.addChild(parseFactor());
    }

    return node;
}

ParseNode Parser::parseFactor() {
    ParseNode node("<factor>");

    if (check(INTCON) || check(REALCON) || check(CHARCON) || check(STRING) ||
        check(BOOLCON)) {
        node.addChild(consume(current().type));
    } else if (check(LPARENT)) {
        node.addChild(consume(LPARENT));
        node.addChild(parseExpression());
        node.addChild(consume(RPARENT));
    } else if (check(NOTSY)) {
        node.addChild(consume(NOTSY));
        node.addChild(parseFactor());
    } else if (check(IDENT) && peek(1).type == LPARENT) {
        node.addChild(parseProcedureOrFunctionCall());
    } else if (check(IDENT) &&
               (peek(1).type == LBRACK || peek(1).type == PERIOD)) {
        node.addChild(parseVariable());
    } else if (check(IDENT)) {
        node.addChild(consume(IDENT));
    } else {
        syntaxError("expected factor");
    }

    return node;
}

ParseNode Parser::parseRelationalOperator() {
    ParseNode node("<relational-operator>");

    if (check(EQL) || check(NEQ) || check(GTR) || check(GEQ) || check(LSS) ||
        check(LEQ)) {
        node.addChild(consume(current().type));
    } else {
        syntaxError("expected relational operator");
    }

    return node;
}

ParseNode Parser::parseAdditiveOperator() {
    ParseNode node("<additive-operator>");

    if (check(PLUS) || check(MINUS) || check(ORSY)) {
        node.addChild(consume(current().type));
    } else {
        syntaxError("expected additive operator");
    }

    return node;
}

ParseNode Parser::parseMultiplicativeOperator() {
    ParseNode node("<multiplicative-operator>");

    if (check(TIMES) || check(IDIV) || check(RDIV) || check(IMOD) ||
        check(ANDSY)) {
        node.addChild(consume(current().type));
    } else {
        syntaxError("expected multiplicative operator");
    }

    return node;
}
