# Arion Interpreter — Milestone 1: Lexical Analysis

IF2224 Teori Bahasa Formal dan Automata | [ISL] TBFO
![cover](cover.png)
## Deskripsi Program

Program saat ini sedang dalam proses Milestone 1, di mana kami membuat **lexical analyzer (lexer)** untuk bahasa pemrograman **Arion**, diimplementasikan dalam C++17 menggunakan **Deterministic Finite Automata (DFA)**. Lexer membaca source code Arion karakter per karakter dan menghasilkan daftar token yang akan digunakan pada tahap selanjutnya (parsing).

Lexer mengenali 52 jenis token meliputi:
- Konstanta literal (integer, real, char, string)
- Identifier dan keywords (kontrol alur, deklarasi)
- Operator (aritmatika, logika, relasional)
- Tanda baca dan simbol struktur
- Komentar (`{ }` dan `(* *)`)

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
./lexer <input.txt>

# Output ke file
./lexer <input.txt> <output.txt>
```

### Contoh

```bash
./lexer test/milestone-1/input-1.txt test/milestone-1/output-1.txt
```

### Clean

```bash
make clean
```

## Struktur Repository

```
.
├── src/
│   ├── main.cpp        # Entry point program
│   ├── lexer.h         # Deklarasi class Lexer
│   ├── lexer.cpp       # Implementasi Lexer (DFA)
│   └── token.h         # Definisi TokenType, Token, helper functions
├── doc/                # Laporan
├── test/
│   └── milestone-1/    # Input dan output pengujian milestone 1
├── Makefile
└── README.md
```

## Pembagian Tugas

| Nama | NIM | Kontribusi |
|------|-----|-----------|
| Narendra Dharma Wistara M. | 13524044 | Infrastructure DFA, literal, identifier, arithmetic |
| Arghawisesa Dwinanda Arham | 13524100 | Keywords kontrol, komentar, error handling |
| Nashiruddin Akram | 13524090 | Tanda baca, simbol struktur, logical operator |
| Steven Tan | 13524060 | Declaration keywords, relational operator |
