#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <string>
#include <vector>

enum class ObjectKind {
    None,
    Reserved,
    Constant,
    Variable,
    Type,
    Procedure,
    Function,
    Program,
    Field,
    Parameter
};

enum class TypeKind {
    None,
    Integer,
    Real,
    Char,
    Boolean,
    String,
    Array,
    Record,
    Subrange,
    Enum
};

enum class BlockKind {
    Program,
    Procedure,
    Function,
    Record,
    Compound
};

struct TabEntry {
    std::string identifier;
    int link;
    ObjectKind obj;
    TypeKind type;
    int ref;
    bool nrm;
    int lev;
    int adr;
    bool initialized;
    std::string literalValue;
};

struct BTabEntry {
    BlockKind kind;
    int last;
    int lpar;
    int psze;
    int vsze;
};

struct ATabEntry {
    TypeKind xtyp;
    TypeKind etyp;
    int eref;
    int low;
    int high;
    int elsz;
    int size;
};

class SymbolTable {
  public:
    SymbolTable();

    void initialize();

    int pushScope(BlockKind kind = BlockKind::Compound);
    void popScope();

    int insert(const std::string &name, ObjectKind obj, TypeKind type,
               int ref = 0, bool nrm = true, int adr = 0,
               bool initialized = false,
               const std::string &literalValue = "");

    int lookup(const std::string &name) const;
    int lookupCurrentScope(const std::string &name) const;

    int insertArray(TypeKind indexType, TypeKind elementType, int elementRef,
                    int low, int high, int elementSize);

    int currentLevel() const;
    int currentBlock() const;

    const TabEntry &tabEntry(int index) const;
    const BTabEntry &btabEntry(int index) const;
    const ATabEntry &atabEntry(int index) const;

    const std::vector<TabEntry> &tab() const;
    const std::vector<BTabEntry> &btab() const;
    const std::vector<ATabEntry> &atab() const;

    std::string renderTab() const;
    std::string renderBTab() const;
    std::string renderATab() const;
    std::string renderAll() const;

    static std::string normalizeIdentifier(const std::string &name);

  private:
    std::vector<TabEntry> tab_;
    std::vector<BTabEntry> btab_;
    std::vector<ATabEntry> atab_;
    std::vector<int> display_;
    int currentLevel_;

    int insertInternal(const std::string &name, ObjectKind obj, TypeKind type,
                       int ref, bool nrm, int adr, bool initialized,
                       const std::string &literalValue,
                       bool checkRedeclaration);

    int currentScopeLast() const;
};

std::string objectKindToString(ObjectKind kind);
std::string typeKindToString(TypeKind kind);
std::string blockKindToString(BlockKind kind);

#endif // SYMBOL_TABLE_H
