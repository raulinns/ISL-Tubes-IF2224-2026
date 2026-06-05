#include "symbol_table.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

TabEntry makeEntry(const std::string &identifier, int link, ObjectKind obj,
                   TypeKind type, int ref, bool nrm, int lev, int adr,
                   bool initialized, const std::string &literalValue) {
    return {identifier, link, obj, type, ref, nrm, lev, adr, initialized,
            literalValue};
}

int typeSize(TypeKind type) {
    switch (type) {
    case TypeKind::Real:
        return 2;
    case TypeKind::Integer:
    case TypeKind::Char:
    case TypeKind::Boolean:
    case TypeKind::String:
    case TypeKind::Array:
    case TypeKind::Record:
    case TypeKind::Subrange:
    case TypeKind::Enum:
        return 1;
    case TypeKind::None:
        return 0;
    }

    return 0;
}

void appendTableDivider(std::ostringstream &out, int width) {
    out << std::string(width, '-') << "\n";
}

} // namespace

SymbolTable::SymbolTable() { initialize(); }

void SymbolTable::initialize() {
    tab_.clear();
    btab_.clear();
    atab_.clear();
    display_.clear();
    currentLevel_ = 0;

    tab_.push_back(makeEntry("<dummy>", 0, ObjectKind::None, TypeKind::None, 0,
                             true, 0, 0, true, ""));

    btab_.push_back({BlockKind::Program, 0, 0, 0, 0});
    display_.push_back(0);

    const std::vector<std::string> reserved = {
        "and",     "array",  "begin",   "case",    "const",   "div",
        "downto",  "do",     "else",    "end",     "for",     "function",
        "if",      "mod",    "not",     "of",      "or",      "procedure",
        "program", "record", "repeat",  "integer", "real",    "boolean",
        "char",    "string", "then",    "to",      "type",    "until",
        "var",     "while"};

    for (const std::string &word : reserved) {
        TypeKind type = TypeKind::None;
        ObjectKind obj = ObjectKind::Reserved;
        if (word == "integer") {
            type = TypeKind::Integer;
            obj = ObjectKind::Type;
        } else if (word == "real") {
            type = TypeKind::Real;
            obj = ObjectKind::Type;
        } else if (word == "boolean") {
            type = TypeKind::Boolean;
            obj = ObjectKind::Type;
        } else if (word == "char") {
            type = TypeKind::Char;
            obj = ObjectKind::Type;
        } else if (word == "string") {
            type = TypeKind::String;
            obj = ObjectKind::Type;
        }

        insertInternal(word, obj, type, 0, true, 0, true, "", false);
    }

    insertInternal("true", ObjectKind::Constant, TypeKind::Boolean, 0, true, 1,
                   true, "true", false);
    insertInternal("false", ObjectKind::Constant, TypeKind::Boolean, 0, true, 0,
                   true, "false", false);
    insertInternal("readln", ObjectKind::Procedure, TypeKind::None, 0, true, 0,
                   true, "", false);
    insertInternal("write", ObjectKind::Procedure, TypeKind::None, 0, true, 0,
                   true, "", false);
    insertInternal("writeln", ObjectKind::Procedure, TypeKind::None, 0, true, 0,
                   true, "", false);
}

int SymbolTable::pushScope(BlockKind kind) {
    btab_.push_back({kind, 0, 0, 0, 0});
    ++currentLevel_;
    display_.push_back(static_cast<int>(btab_.size()) - 1);
    return currentBlock();
}

void SymbolTable::popScope() {
    if (currentLevel_ == 0) {
        throw std::runtime_error("Cannot pop global scope");
    }

    display_.pop_back();
    --currentLevel_;
}

int SymbolTable::insert(const std::string &name, ObjectKind obj, TypeKind type,
                        int ref, bool nrm, int adr, bool initialized,
                        const std::string &literalValue) {
    return insertInternal(name, obj, type, ref, nrm, adr, initialized,
                          literalValue, true);
}

int SymbolTable::insertInternal(const std::string &name, ObjectKind obj,
                                TypeKind type, int ref, bool nrm, int adr,
                                bool initialized,
                                const std::string &literalValue,
                                bool checkRedeclaration) {
    const std::string normalized = normalizeIdentifier(name);
    if (normalized.empty()) {
        throw std::runtime_error("Cannot insert empty identifier");
    }

    if (checkRedeclaration && lookupCurrentScope(normalized) >= 0) {
        throw std::runtime_error("Redeclaration of identifier: " + name);
    }

    const int blockIndex = currentBlock();
    const int previous = btab_[blockIndex].last;
    const int index = static_cast<int>(tab_.size());

    tab_.push_back(makeEntry(normalized, previous, obj, type, ref, nrm,
                             currentLevel_, adr, initialized, literalValue));
    btab_[blockIndex].last = index;

    if (obj == ObjectKind::Variable || obj == ObjectKind::Parameter ||
        obj == ObjectKind::Field) {
        if (obj == ObjectKind::Parameter) {
            btab_[blockIndex].psze += typeSize(type);
            btab_[blockIndex].lpar = index;
        } else {
            btab_[blockIndex].vsze += typeSize(type);
        }
    }

    return index;
}

int SymbolTable::lookup(const std::string &name) const {
    const std::string normalized = normalizeIdentifier(name);
    for (int level = currentLevel_; level >= 0; --level) {
        int index = btab_[display_[level]].last;
        while (index > 0) {
            const TabEntry &entry = tab_[index];
            if (entry.identifier == normalized) {
                return index;
            }
            index = entry.link;
        }
    }
    return -1;
}

int SymbolTable::lookupCurrentScope(const std::string &name) const {
    const std::string normalized = normalizeIdentifier(name);
    int index = currentScopeLast();
    while (index > 0) {
        const TabEntry &entry = tab_[index];
        if (entry.identifier == normalized) {
            return index;
        }
        index = entry.link;
    }
    return -1;
}

void SymbolTable::markInitialized(int tabIndex) {
    if (tabIndex < 0 || tabIndex >= static_cast<int>(tab_.size())) {
        throw std::out_of_range("tab index out of range");
    }
    tab_[tabIndex].initialized = true;
}

void SymbolTable::setReference(int tabIndex, int ref) {
    if (tabIndex < 0 || tabIndex >= static_cast<int>(tab_.size())) {
        throw std::out_of_range("tab index out of range");
    }
    tab_[tabIndex].ref = ref;
}

int SymbolTable::insertArray(TypeKind indexType, TypeKind elementType,
                             int elementRef, int low, int high,
                             int elementSize) {
    if (indexType != TypeKind::Integer && indexType != TypeKind::Char &&
        indexType != TypeKind::Boolean && indexType != TypeKind::Subrange &&
        indexType != TypeKind::Enum) {
        throw std::runtime_error(
            "Array index type must be a simple ordinal type and cannot be Real");
    }
    if (low > high) {
        throw std::runtime_error("Array lower bound cannot exceed upper bound");
    }
    if (elementSize < 0) {
        throw std::runtime_error("Array element size cannot be negative");
    }

    const int size = (high - low + 1) * elementSize;
    atab_.push_back(
        {indexType, elementType, elementRef, low, high, elementSize, size});
    return static_cast<int>(atab_.size()) - 1;
}

int SymbolTable::currentLevel() const { return currentLevel_; }

int SymbolTable::currentBlock() const {
    if (display_.empty()) {
        throw std::runtime_error("Symbol table has no active scope");
    }
    return display_.back();
}

const TabEntry &SymbolTable::tabEntry(int index) const {
    if (index < 0 || index >= static_cast<int>(tab_.size())) {
        throw std::out_of_range("tab index out of range");
    }
    return tab_[index];
}

const BTabEntry &SymbolTable::btabEntry(int index) const {
    if (index < 0 || index >= static_cast<int>(btab_.size())) {
        throw std::out_of_range("btab index out of range");
    }
    return btab_[index];
}

const ATabEntry &SymbolTable::atabEntry(int index) const {
    if (index < 0 || index >= static_cast<int>(atab_.size())) {
        throw std::out_of_range("atab index out of range");
    }
    return atab_[index];
}

const std::vector<TabEntry> &SymbolTable::tab() const { return tab_; }

const std::vector<BTabEntry> &SymbolTable::btab() const { return btab_; }

const std::vector<ATabEntry> &SymbolTable::atab() const { return atab_; }

std::string SymbolTable::renderTab() const {
    std::ostringstream out;
    out << "tab\n";
    appendTableDivider(out, 94);
    out << std::left << std::setw(5) << "idx" << std::setw(18) << "identifier"
        << std::setw(12) << "obj" << std::setw(10) << "type" << std::setw(6)
        << "ref" << std::setw(6) << "nrm" << std::setw(6) << "lev"
        << std::setw(6) << "adr" << std::setw(6) << "link" << std::setw(7)
        << "init" << "value"
        << "\n";
    appendTableDivider(out, 94);

    for (std::size_t i = 0; i < tab_.size(); ++i) {
        const TabEntry &entry = tab_[i];
        out << std::left << std::setw(5) << i << std::setw(18)
            << entry.identifier << std::setw(12)
            << objectKindToString(entry.obj) << std::setw(10)
            << typeKindToString(entry.type) << std::setw(6) << entry.ref
            << std::setw(6) << (entry.nrm ? 1 : 0) << std::setw(6)
            << entry.lev << std::setw(6) << entry.adr << std::setw(6)
            << entry.link << std::setw(7) << (entry.initialized ? 1 : 0)
            << entry.literalValue << "\n";
    }

    return out.str();
}

std::string SymbolTable::renderBTab() const {
    std::ostringstream out;
    out << "btab\n";
    appendTableDivider(out, 48);
    out << std::left << std::setw(5) << "idx" << std::setw(12) << "kind"
        << std::setw(7) << "last" << std::setw(7) << "lpar" << std::setw(7)
        << "psze" << "vsze"
        << "\n";
    appendTableDivider(out, 48);

    for (std::size_t i = 0; i < btab_.size(); ++i) {
        const BTabEntry &entry = btab_[i];
        out << std::left << std::setw(5) << i << std::setw(12)
            << blockKindToString(entry.kind) << std::setw(7) << entry.last
            << std::setw(7) << entry.lpar << std::setw(7) << entry.psze
            << entry.vsze << "\n";
    }

    return out.str();
}

std::string SymbolTable::renderATab() const {
    std::ostringstream out;
    out << "atab\n";
    appendTableDivider(out, 62);
    out << std::left << std::setw(5) << "idx" << std::setw(10) << "xtyp"
        << std::setw(10) << "etyp" << std::setw(7) << "eref" << std::setw(7)
        << "low" << std::setw(7) << "high" << std::setw(7) << "elsz"
        << "size"
        << "\n";
    appendTableDivider(out, 62);

    for (std::size_t i = 0; i < atab_.size(); ++i) {
        const ATabEntry &entry = atab_[i];
        out << std::left << std::setw(5) << i << std::setw(10)
            << typeKindToString(entry.xtyp) << std::setw(10)
            << typeKindToString(entry.etyp) << std::setw(7) << entry.eref
            << std::setw(7) << entry.low << std::setw(7) << entry.high
            << std::setw(7) << entry.elsz << entry.size << "\n";
    }

    return out.str();
}

std::string SymbolTable::renderAll() const {
    std::ostringstream out;
    out << renderTab() << "\n" << renderBTab() << "\n" << renderATab();
    return out.str();
}

std::string SymbolTable::normalizeIdentifier(const std::string &name) {
    std::string normalized = name;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return normalized;
}

int SymbolTable::currentScopeLast() const { return btab_[currentBlock()].last; }

std::string objectKindToString(ObjectKind kind) {
    switch (kind) {
    case ObjectKind::None:
        return "none";
    case ObjectKind::Reserved:
        return "reserved";
    case ObjectKind::Constant:
        return "constant";
    case ObjectKind::Variable:
        return "variable";
    case ObjectKind::Type:
        return "type";
    case ObjectKind::Procedure:
        return "procedure";
    case ObjectKind::Function:
        return "function";
    case ObjectKind::Program:
        return "program";
    case ObjectKind::Field:
        return "field";
    case ObjectKind::Parameter:
        return "parameter";
    }

    return "unknown";
}

std::string typeKindToString(TypeKind kind) {
    switch (kind) {
    case TypeKind::None:
        return "none";
    case TypeKind::Integer:
        return "integer";
    case TypeKind::Real:
        return "real";
    case TypeKind::Char:
        return "char";
    case TypeKind::Boolean:
        return "boolean";
    case TypeKind::String:
        return "string";
    case TypeKind::Array:
        return "array";
    case TypeKind::Record:
        return "record";
    case TypeKind::Subrange:
        return "subrange";
    case TypeKind::Enum:
        return "enum";
    }

    return "unknown";
}

std::string blockKindToString(BlockKind kind) {
    switch (kind) {
    case BlockKind::Program:
        return "program";
    case BlockKind::Procedure:
        return "procedure";
    case BlockKind::Function:
        return "function";
    case BlockKind::Record:
        return "record";
    case BlockKind::Compound:
        return "compound";
    }

    return "unknown";
}
