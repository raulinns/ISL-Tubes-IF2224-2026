# Arion Compiler — Milestone 2: Syntax Analysis

IF2224 Teori Bahasa Formal dan Automata | [ISL] TBFO
![](Cover.jpg)

## Deskripsi Program

Program ini merupakan implementasi **Milestone 2 (Syntax Analysis / Parser)** dari compiler untuk bahasa pemrograman **Arion**. Parser dibangun menggunakan algoritma **Recursive Descent** yang membaca daftar token hasil dari lexer (Milestone 1) dan membangun **Parse Tree** yang merepresentasikan struktur hierarkis program sesuai grammar bahasa Arion.

Fitur utama program:

- **Lexical Analysis (Milestone 1)**: Membaca source code Arion dan menghasilkan daftar token menggunakan DFA
- **Syntax Analysis (Milestone 2)**: Menganalisis urutan token dan membangun Parse Tree menggunakan Recursive Descent
- **Error Handling**: Mendeteksi dan melaporkan kesalahan leksikal maupun sintaksis dengan pesan informatif
- **Output Parse Tree**: Menampilkan Parse Tree ke terminal atau menyimpannya ke file `.txt`

Grammar yang didukung mencakup:
- Deklarasi (`const`, `type`, `var`, `procedure`, `function`)
- Tipe data (identifier, array, range, enumerated, record)
- Statement kontrol (`if-then-else`, `case`, `while`, `repeat-until`, `for`)
- Ekspresi (aritmatika, relasional, logika)
- Pemanggilan prosedur/fungsi

## Requirements

- **Compiler**: g++ dengan dukungan C++17
- **OS**: Linux / macOS (untuk Windows harus menggunakan WSL)
- **Build tool**: GNU Make

## Cara Instalasi dan Penggunaan

### Build

```bash
make
```

### Run

```bash
# Output ke terminal
./arion <input.txt>

# Output ke file
./arion <input.txt> <output.txt>
```

### Contoh

```bash
# Milestone 2 — Syntax Analysis
./arion test/milestone-2/test-1.txt test/milestone-2/output-1.txt
```

### Clean

```bash
make clean
```

## Struktur Repository

```
.
├── src/
│   ├── main.cpp           # Entry point program (lexer + parser)
│   ├── lexer.h            # Deklarasi class Lexer
│   ├── lexer.cpp          # Implementasi Lexer (DFA)
│   ├── token.h            # Definisi TokenType, Token, helper functions
│   ├── parser.h           # Deklarasi class Parser (Recursive Descent)
│   ├── parser.cpp         # Implementasi Parser
│   ├── parse_tree.h       # Definisi struct ParseNode
│   └── parse_tree.cpp     # Implementasi render Parse Tree
├── doc/                   # Laporan
├── test/
│   ├── milestone-1/       # Input dan output pengujian milestone 1
│   └── milestone-2/       # Input dan output pengujian milestone 2
├── Makefile
└── README.md
```

## Pembagian Tugas

| Nama                       | NIM      | Kontribusi                                                        |
| -------------------------- | -------- | ----------------------------------------------------------------- |
| Narendra Dharma Wistara M. | 13524044 | Program, deklarasi (const, type, var), tipe data (array, range, enumerated, record) |
| Arghawisesa Dwinanda Arham | 13524100 | Subprogram (procedure, function), statement kontrol (if, case, while, repeat, for) |
| Nashiruddin Akram          | 13524090 | Statement dasar & compound (compound-statement, statement-list, assignment, procedure/function-call) |
| Steven Tan                 | 13524060 | Ekspresi (expression, simple-expression, term, factor, operator relasional/aditif/multiplikatif) |
