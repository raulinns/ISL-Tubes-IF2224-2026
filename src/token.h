#ifndef TOKEN_H
#define TOKEN_H

#include <string>

enum TokenType {
    // Literal Value
    INTCON,         // No.1  : Konstanta integer          contoh: 1, 3, 48
    REALCON,        // No.2  : Konstanta bilangan riil     contoh: 3.14, 26.7
    CHARCON,        // No.3  : Konstanta karakter tunggal  contoh: 'j', 'k'
    STRING,         // No.4  : Sekuens karakter            contoh: 'IRK', 'TBFO'

    // Identifier
    IDENT,          // No.37 : x, PI, MyInt, h4g, ...
    
    // Logical Operator
    NOTSY,          // No.5  : NOT
    ANDSY,          // No.12 : AND
    ORSY,           // No.13 : OR

    // Relational Operator
    EQL,            // No.14 : ==
    NEQ,            // No.15 : <>
    GTR,            // No.16 : >
    GEQ,            // No.17 : >=
    LSS,            // No.18 : <
    LEQ,            // No.19 : <=

    // Arithmatic Operator
    PLUS,           // No.6  : +
    MINUS,          // No.7  : -
    TIMES,          // No.8  : *
    IDIV,           // No.9  : div  (pembagian integer)
    RDIV,           // No.10 : /    (pembagian riil)
    IMOD,           // No.11 : MOD


    // Brackets, Tanda Baca
    LPARENT,        // No.20 : (
    RPARENT,        // No.21 : )
    LBRACK,         // No.22 : [
    RBRACK,         // No.23 : ]
    COMMA,          // No.24 : ,
    SEMICOLON,      // No.25 : ;
    PERIOD,         // No.26 : .
    COLON,          // No.27 : :
    BECOMES,        // No.28 : :=   (assignment)

    // Declaration Keywords
    CONSTSY,        // No.29 : const
    TYPESY,         // No.30 : type
    VARSY,          // No.31 : var
    FUNCTIONSY,     // No.32 : function
    PROCEDURESY,    // No.33 : procedure
    ARRAYSY,        // No.34 : array
    RECORDSY,       // No.35 : record
    PROGRAMSY,      // No.36 : program


    // Keywords
    BEGINSY,        // No.38 : begin
    IFSY,           // No.39 : if
    CASESY,         // No.40 : case
    REPEATSY,       // No.41 : repeat
    WHILESY,        // No.42 : while
    FORSY,          // No.43 : for
    ENDSY,          // No.44 : end
    ELSESY,         // No.45 : else
    UNTILSY,        // No.46 : until
    OFSY,           // No.47 : of
    DOSY,           // No.48 : do
    TOSY,           // No.49 : to
    DOWNTOSY,       // No.50 : downto
    THENSY,         // No.51 : then

    // Komentar
    // { ... }  atau  (* ... *)
    COMMENT,        // No.52 : komentar (umumnya dibuang, tetap dikenali)

    TOKEN_ERROR     // Karakter/pola yang tidak dikenal
};

struct Token {
    TokenType   type;
    std::string value;      // Identifier value
    int         line;       // Untuk error report

    // Constructor
    Token(TokenType t, const std::string& v = "", int ln = 0)
        : type(t), value(v), line(ln) {}
};

// Digunakan untuk output file sesuai TokenType
inline std::string tokenTypeToString(TokenType t) {
    switch (t) {
        // Literal
        case INTCON:      return "intcon";
        case REALCON:     return "realcon";
        case CHARCON:     return "charcon";
        case STRING:      return "string";

        // Operator logika
        case NOTSY:       return "notsy";
        case ANDSY:       return "andsy";
        case ORSY:        return "orsy";

        // Operator aritmatika
        case PLUS:        return "plus";
        case MINUS:       return "minus";
        case TIMES:       return "times";
        case IDIV:        return "idiv";
        case RDIV:        return "rdiv";
        case IMOD:        return "imod";

        // Operator perbandingan
        case EQL:         return "eql";
        case NEQ:         return "neq";
        case GTR:         return "gtr";
        case GEQ:         return "geq";
        case LSS:         return "lss";
        case LEQ:         return "leq";

        // Tanda baca
        case LPARENT:     return "lparent";
        case RPARENT:     return "rparent";
        case LBRACK:      return "lbrack";
        case RBRACK:      return "rbrack";
        case COMMA:       return "comma";
        case SEMICOLON:   return "semicolon";
        case PERIOD:      return "period";
        case COLON:       return "colon";
        case BECOMES:     return "becomes";

        // Keyword deklarasi
        case CONSTSY:     return "constsy";
        case TYPESY:      return "typesy";
        case VARSY:       return "varsy";
        case FUNCTIONSY:  return "functionsy";
        case PROCEDURESY: return "proceduresy";
        case ARRAYSY:     return "arraysy";
        case RECORDSY:    return "recordsy";
        case PROGRAMSY:   return "programsy";

        // Identifier
        case IDENT:       return "ident";

        // Keyword kontrol
        case BEGINSY:     return "beginsy";
        case IFSY:        return "ifsy";
        case CASESY:      return "casesy";
        case REPEATSY:    return "repeatsy";
        case WHILESY:     return "whilesy";
        case FORSY:       return "forsy";
        case ENDSY:       return "endsy";
        case ELSESY:      return "elsesy";
        case UNTILSY:     return "untilsy";
        case OFSY:        return "ofsy";
        case DOSY:        return "dosy";
        case TOSY:        return "tosy";
        case DOWNTOSY:    return "downtosy";
        case THENSY:      return "thensy";

        // Komentar & error
        case COMMENT:     return "comment";
        case TOKEN_ERROR: return "unknown";

        default:          return "unknown";
    }
}

// Fungsi bantuan untuk token-token yang membutuhkan value
inline bool tokenNeedsValue(TokenType t) {
    switch (t) {
        case INTCON:
        case REALCON:
        case CHARCON:
        case STRING:
        case IDENT:
        case COMMENT:
        case TOKEN_ERROR:
            return true;
        default:
            return false;
    }
}

#endif // TOKEN_H
