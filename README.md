# Arion Compiler - Milestone 3: Semantic Analysis

IF2224 Teori Bahasa Formal dan Automata | [ISL] TBFO

![Cover](Cover.jpg)

## Deskripsi Program

Program ini merupakan implementasi compiler bahasa Arion sampai tahap **Milestone 3 (Semantic Analysis)**. Alur program membaca source code Arion atau parse tree hasil parser, membangun AST, menjalankan semantic visitor, lalu menghasilkan **Decorated AST**, **symbol table**, dan daftar diagnostic semantic.

Fitur utama:

- **Lexical Analysis**: membaca source code Arion dan menghasilkan token berbasis DFA.
- **Syntax Analysis**: memvalidasi token dengan recursive descent parser dan membangun parse tree.
- **AST Builder**: mengubah parse tree menjadi AST yang lebih ringkas.
- **Semantic Analysis**: melakukan pengecekan deklarasi, scope, tipe ekspresi, assignment, kondisi kontrol, array index, record field, parameter call, dan return function.
- **Symbol Table**: mengelola `tab`, `btab`, dan `atab` beserta operasi `lookup`, `insert`, `pushScope`, dan `popScope`.
- **Output**: menampilkan decorated AST, symbol table, dan semantic diagnostics.

## Requirements

- Compiler: `g++` dengan dukungan C++17
- Build tool: GNU Make
- OS: Linux atau macOS

## Cara Build dan Run

```bash
make
```

Menjalankan dari source code Arion:

```bash
./arion <input.txt>
./arion <input.txt> <output.txt>
```

Program juga dapat menerima parse tree ter-render hasil parser, selama file diawali root `<program>` dengan format tree ASCII yang dihasilkan `renderParseTree`.

Contoh:

```bash
./arion test/input.txt
./arion test/input.txt output.txt
```

Membersihkan build:

```bash
make clean
```

## Struktur Repository

```text
.
├── src/
│   ├── main.cpp                 # Entry point dan integrasi lexer-parser-AST-semantic
│   ├── lexer.h/.cpp             # Lexical analyzer
│   ├── token.h                  # Definisi token
│   ├── parser.h/.cpp            # Recursive descent parser
│   ├── parse_tree.h/.cpp        # Struktur, render, dan pembacaan parse tree
│   ├── ast.h/.cpp               # Struktur dan render AST
│   ├── ast_builder.h/.cpp       # Konversi parse tree ke AST
│   ├── symbol_table.h/.cpp      # tab, btab, atab, scope stack, lookup/insert
│   └── semantic_analyzer.h/.cpp # Semantic visitor dan type checking
├── doc/
├── test/
├── Makefile
└── README.md
```

## Pembagian Tugas Milestone 3

| Nama | Kontribusi |
| --- | --- |
| Akram | Membuat AST dari parse tree, termasuk definisi node AST dan converter parse tree ke AST. |
| Endra | Membuat `tab`, `btab`, `atab`, scope stack, predefined identifier, dan fungsi `lookup`, `insert`, `push`, `pop`. |
| Argha | Membuat aturan semantic dan type checking, terutama ekspresi, assignment, kondisi `if`/`while`, array index, dan compatibility type. |
| Steven | Membuat semantic visitor dan integrasi ke `main`, output decorated AST/symbol table, test end-to-end, dan error reporting. |
