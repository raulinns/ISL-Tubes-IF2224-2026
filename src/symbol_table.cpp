#include "symbol_table.h"
#include "ast.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

TabEntry makeEntry(const std::string &identifier, int link, ObjectKind obj,
                   TypeKind type, int ref, bool nrm, int lev, int adr,
                   bool initialized, const std::string &literalValue) {
    return {identifier, link, obj, type, ref, ref, nrm, lev, adr, initialized,
            literalValue};
}

int primitiveTypeSize(TypeKind type) {
    switch (type) {
    case TypeKind::Integer:
    case TypeKind::Real:
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

std::string trim(const std::string &text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::vector<std::string> splitLines(const std::string &text) {
    std::vector<std::string> lines;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::string sliceColumn(const std::string &line, std::size_t start,
                        std::size_t width) {
    if (start >= line.size()) {
        return "";
    }
    return trim(line.substr(start, width));
}

int parseIntColumn(const std::string &line, std::size_t start, std::size_t width) {
    const std::string value = sliceColumn(line, start, width);
    return value.empty() ? 0 : std::stoi(value);
}

bool parseBoolColumn(const std::string &line, std::size_t start, std::size_t width) {
    return parseIntColumn(line, start, width) != 0;
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
        int size = primitiveTypeSize(type);
        if (type == TypeKind::Array && ref >= 0 &&
            ref < static_cast<int>(atab_.size())) {
            size = atab_[static_cast<std::size_t>(ref)].size;
        } else if (type == TypeKind::Record && ref >= 0 &&
                   ref < static_cast<int>(btab_.size())) {
            size = btab_[static_cast<std::size_t>(ref)].vsze;
        }
        if (obj == ObjectKind::Parameter) {
            btab_[blockIndex].psze += size;
            btab_[blockIndex].lpar = index;
        } else {
            btab_[blockIndex].vsze += size;
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

TabEntry &SymbolTable::mutableTabEntry(int index) {
    if (index < 0 || index >= static_cast<int>(tab_.size())) {
        throw std::out_of_range("tab index out of range");
    }
    return tab_[index];
}

BTabEntry &SymbolTable::mutableBtabEntry(int index) {
    if (index < 0 || index >= static_cast<int>(btab_.size())) {
        throw std::out_of_range("btab index out of range");
    }
    return btab_[index];
}

ATabEntry &SymbolTable::mutableAtabEntry(int index) {
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
    appendTableDivider(out, 100);
    out << std::left << std::setw(5) << "idx" << std::setw(18) << "identifier"
        << std::setw(12) << "obj" << std::setw(10) << "type" << std::setw(6)
        << "ref" << std::setw(6) << "tref" << std::setw(6) << "nrm"
        << std::setw(6) << "lev"
        << std::setw(6) << "adr" << std::setw(6) << "link" << std::setw(7)
        << "init" << "value"
        << "\n";
    appendTableDivider(out, 100);

    for (std::size_t i = 0; i < tab_.size(); ++i) {
        const TabEntry &entry = tab_[i];
        out << std::left << std::setw(5) << i << std::setw(18)
            << entry.identifier << std::setw(12)
            << objectKindToString(entry.obj) << std::setw(10)
            << typeKindToString(entry.type) << std::setw(6) << entry.ref
            << std::setw(6) << entry.typeRef << std::setw(6)
            << (entry.nrm ? 1 : 0) << std::setw(6)
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

void SymbolTable::loadRenderedAll(const std::string &text) {
    const std::vector<std::string> lines = splitLines(text);
    std::size_t index = 0;

    auto skipBlankLines = [&]() {
        while (index < lines.size() && trim(lines[index]).empty()) {
            ++index;
        }
    };
    auto skipDivider = [&]() {
        if (index < lines.size() &&
            trim(lines[index]).find_first_not_of('-') == std::string::npos) {
            ++index;
        }
    };

    skipBlankLines();
    if (index >= lines.size() || trim(lines[index]) != "tab") {
        throw std::runtime_error("Rendered symbol table must start with tab section");
    }
    ++index;
    skipDivider();
    if (index >= lines.size()) {
        throw std::runtime_error("Missing tab header row");
    }
    const bool hasTypeRef = lines[index].find("tref") != std::string::npos;
    ++index;
    skipDivider();

    std::vector<TabEntry> parsedTab;
    while (index < lines.size()) {
        const std::string line = lines[index];
        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            ++index;
            break;
        }
        if (trimmed == "btab") {
            break;
        }

        TabEntry entry;
        entry.identifier = sliceColumn(line, 5, 18);
        entry.obj = objectKindFromString(sliceColumn(line, 23, 12));
        entry.type = typeKindFromString(sliceColumn(line, 35, 10));
        entry.ref = parseIntColumn(line, 45, 6);
        entry.typeRef =
            hasTypeRef ? parseIntColumn(line, 51, 6) : entry.ref;
        const std::size_t shift = hasTypeRef ? 6U : 0U;
        entry.nrm = parseBoolColumn(line, 51 + shift, 6);
        entry.lev = parseIntColumn(line, 57 + shift, 6);
        entry.adr = parseIntColumn(line, 63 + shift, 6);
        entry.link = parseIntColumn(line, 69 + shift, 6);
        entry.initialized = parseBoolColumn(line, 75 + shift, 7);
        entry.literalValue =
            line.size() > 82 + shift
                ? trim(line.substr(82 + shift))
                : std::string();
        parsedTab.push_back(entry);
        ++index;
    }

    skipBlankLines();
    if (index >= lines.size() || trim(lines[index]) != "btab") {
        throw std::runtime_error("Rendered symbol table is missing btab section");
    }
    ++index;
    skipDivider();
    if (index >= lines.size()) {
        throw std::runtime_error("Missing btab header row");
    }
    ++index;
    skipDivider();

    std::vector<BTabEntry> parsedBtab;
    while (index < lines.size()) {
        const std::string line = lines[index];
        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            ++index;
            break;
        }
        if (trimmed == "atab") {
            break;
        }

        BTabEntry entry;
        entry.kind = blockKindFromString(sliceColumn(line, 5, 12));
        entry.last = parseIntColumn(line, 17, 7);
        entry.lpar = parseIntColumn(line, 24, 7);
        entry.psze = parseIntColumn(line, 31, 7);
        entry.vsze = parseIntColumn(line, 38, 10);
        parsedBtab.push_back(entry);
        ++index;
    }

    skipBlankLines();
    if (index >= lines.size() || trim(lines[index]) != "atab") {
        throw std::runtime_error("Rendered symbol table is missing atab section");
    }
    ++index;
    skipDivider();
    if (index >= lines.size()) {
        throw std::runtime_error("Missing atab header row");
    }
    ++index;
    skipDivider();

    std::vector<ATabEntry> parsedAtab;
    while (index < lines.size()) {
        const std::string line = lines[index];
        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            ++index;
            continue;
        }

        ATabEntry entry;
        entry.xtyp = typeKindFromString(sliceColumn(line, 5, 10));
        entry.etyp = typeKindFromString(sliceColumn(line, 15, 10));
        entry.eref = parseIntColumn(line, 25, 7);
        entry.low = parseIntColumn(line, 32, 7);
        entry.high = parseIntColumn(line, 39, 7);
        entry.elsz = parseIntColumn(line, 46, 7);
        entry.size = parseIntColumn(line, 53, 9);
        parsedAtab.push_back(entry);
        ++index;
    }

    tab_ = std::move(parsedTab);
    btab_ = std::move(parsedBtab);
    atab_ = std::move(parsedAtab);
    display_.clear();
    display_.push_back(0);
    currentLevel_ = 0;
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

ObjectKind objectKindFromString(const std::string &name) {
    if (name == "none") {
        return ObjectKind::None;
    }
    if (name == "reserved") {
        return ObjectKind::Reserved;
    }
    if (name == "constant") {
        return ObjectKind::Constant;
    }
    if (name == "variable") {
        return ObjectKind::Variable;
    }
    if (name == "type") {
        return ObjectKind::Type;
    }
    if (name == "procedure") {
        return ObjectKind::Procedure;
    }
    if (name == "function") {
        return ObjectKind::Function;
    }
    if (name == "program") {
        return ObjectKind::Program;
    }
    if (name == "field") {
        return ObjectKind::Field;
    }
    if (name == "parameter") {
        return ObjectKind::Parameter;
    }
    throw std::runtime_error("Unknown object kind: " + name);
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

TypeKind typeKindFromString(const std::string &name) {
    if (name == "none") {
        return TypeKind::None;
    }
    if (name == "integer") {
        return TypeKind::Integer;
    }
    if (name == "real") {
        return TypeKind::Real;
    }
    if (name == "char") {
        return TypeKind::Char;
    }
    if (name == "boolean") {
        return TypeKind::Boolean;
    }
    if (name == "string") {
        return TypeKind::String;
    }
    if (name == "array") {
        return TypeKind::Array;
    }
    if (name == "record") {
        return TypeKind::Record;
    }
    if (name == "subrange") {
        return TypeKind::Subrange;
    }
    if (name == "enum") {
        return TypeKind::Enum;
    }
    throw std::runtime_error("Unknown type kind: " + name);
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

BlockKind blockKindFromString(const std::string &name) {
    if (name == "program") {
        return BlockKind::Program;
    }
    if (name == "procedure") {
        return BlockKind::Procedure;
    }
    if (name == "function") {
        return BlockKind::Function;
    }
    if (name == "record") {
        return BlockKind::Record;
    }
    if (name == "compound") {
        return BlockKind::Compound;
    }
    throw std::runtime_error("Unknown block kind: " + name);
}

namespace {

int checkedProduct(int left, int right) {
    const long long value = static_cast<long long>(left) * right;
    if (value < 0 || value > 2147483647LL) {
        throw std::runtime_error("Composite type size exceeds runtime limits");
    }
    return static_cast<int>(value);
}

int recordSize(int blockRef, const SymbolTable &symbols) {
    const BTabEntry &block = symbols.btabEntry(blockRef);
    std::vector<int> fields;
    for (int index = block.last; index > 0; index = symbols.tabEntry(index).link) {
        if (symbols.tabEntry(index).obj == ObjectKind::Field) {
            fields.push_back(index);
        }
    }
    int size = 0;
    for (auto it = fields.rbegin(); it != fields.rend(); ++it) {
        const TabEntry &field = symbols.tabEntry(*it);
        size += runtimeTypeSize(field.type, field.typeRef, symbols);
    }
    return size;
}

void normalizeCompositeType(TypeKind type, int ref, SymbolTable &symbols);

void normalizeArray(int arrayRef, SymbolTable &symbols) {
    ATabEntry &array = symbols.mutableAtabEntry(arrayRef);
    // An array element can be a record, so calculate its field offsets first before its size.
    normalizeCompositeType(array.etyp, array.eref, symbols);
    array.elsz = runtimeTypeSize(array.etyp, array.eref, symbols);
    array.size = checkedProduct(array.high - array.low + 1, array.elsz);
}

void normalizeRecord(int blockRef, SymbolTable &symbols) {
    BTabEntry &block = symbols.mutableBtabEntry(blockRef);
    std::vector<int> fields;
    for (int index = block.last; index > 0;
         index = symbols.tabEntry(index).link) {
        if (symbols.tabEntry(index).obj == ObjectKind::Field) {
            fields.push_back(index);
        }
    }
    int offset = 0;
    for (auto it = fields.rbegin(); it != fields.rend(); ++it) {
        TabEntry &field = symbols.mutableTabEntry(*it);
        normalizeCompositeType(field.type, field.typeRef, symbols);
        field.adr = offset;
        offset += runtimeTypeSize(field.type, field.typeRef, symbols);
    }
    block.psze = 0;
    block.vsze = offset;
}

void normalizeCompositeType(TypeKind type, int ref, SymbolTable &symbols) {
    if (ref < 0) {
        return;
    }
    if (type == TypeKind::Array) {
        normalizeArray(ref, symbols);
    } else if (type == TypeKind::Record) {
        normalizeRecord(ref, symbols);
    }
}

int layoutDeclarations(const AstNode &declarations, SymbolTable &symbols,
                       int initialOffset);

void layoutSubprogram(const AstNode &decl, SymbolTable &symbols) {
    if (decl.symbolIndex < 0) {
        return;
    }
    TabEntry &callable = symbols.mutableTabEntry(decl.symbolIndex);
    int offset = 3;
    int parameterSize = 0;
    if (!decl.children.empty() && decl.children[0].kind == AstKind::Parameters) {
        for (const AstNode &param : decl.children[0].children) {
            if (param.symbolIndex < 0) {
                continue;
            }
            TabEntry &entry = symbols.mutableTabEntry(param.symbolIndex);
            entry.adr = offset;
            normalizeCompositeType(entry.type, entry.typeRef, symbols);
            const int size =
                runtimeTypeSize(entry.type, entry.typeRef, symbols);
            offset += size;
            parameterSize += size;
        }
    }

    if (decl.kind == AstKind::FunctionDecl) {
        callable.adr = offset;
        normalizeCompositeType(callable.type, callable.typeRef, symbols);
        offset += runtimeTypeSize(callable.type, callable.typeRef, symbols);
    }

    const std::size_t declarationsIndex =
        decl.kind == AstKind::FunctionDecl ? 2U : 1U;
    if (decl.children.size() > declarationsIndex) {
        offset = layoutDeclarations(decl.children[declarationsIndex], symbols,
                                    offset);
    }
    if (callable.ref >= 0 &&
        callable.ref < static_cast<int>(symbols.btab().size())) {
        BTabEntry &block = symbols.mutableBtabEntry(callable.ref);
        block.psze = parameterSize;
        block.vsze = offset - 3 - parameterSize;
    }
}

int layoutDeclarations(const AstNode &declarations, SymbolTable &symbols,
                       int initialOffset) {
    int offset = initialOffset;
    if (declarations.kind != AstKind::Declarations) {
        return offset;
    }
    for (const AstNode &decl : declarations.children) {
        if (decl.kind == AstKind::VarDecl && decl.symbolIndex >= 0) {
            TabEntry &entry = symbols.mutableTabEntry(decl.symbolIndex);
            normalizeCompositeType(entry.type, entry.typeRef, symbols);
            entry.adr = offset;
            offset += runtimeTypeSize(entry.type, entry.typeRef, symbols);
        }
    }
    for (const AstNode &decl : declarations.children) {
        if (decl.kind == AstKind::ProcedureDecl ||
            decl.kind == AstKind::FunctionDecl) {
            layoutSubprogram(decl, symbols);
        }
    }
    return offset;
}

} // namespace

int runtimeTypeSize(TypeKind type, int ref, const SymbolTable &symbols) {
    switch (type) {
    case TypeKind::None:
        return 0;
    case TypeKind::Array:
        return symbols.atabEntry(ref).size;
    case TypeKind::Record:
        return recordSize(ref, symbols);
    case TypeKind::Integer:
    case TypeKind::Real:
    case TypeKind::Char:
    case TypeKind::Boolean:
    case TypeKind::String:
    case TypeKind::Subrange:
    case TypeKind::Enum:
        return 1;
    }
    return 0;
}

void normalizeRuntimeLayout(const AstNode &ast, SymbolTable &symbols) {
    if (ast.kind != AstKind::Program) {
        throw std::runtime_error("Runtime layout requires a Program AST root");
    }
    int finalOffset = 3;
    if (!ast.children.empty()) {
        finalOffset = layoutDeclarations(ast.children[0], symbols, 3);
    }
    if (!symbols.btab().empty()) {
        BTabEntry &program = symbols.mutableBtabEntry(0);
        program.psze = 0;
        program.vsze = finalOffset - 3;
    }
}
