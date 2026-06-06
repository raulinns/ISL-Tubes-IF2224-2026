# Arion Compiler - Milestone 4

Implementasi compiler dan interpreter Stack Machine untuk bahasa Arion pada
Tugas Besar IF2224 Teori Bahasa Formal dan Otomata.

Pipeline lengkap tersedia untuk pengembangan:

```text
Source -> Lexer -> Parser -> AST -> Semantic Analysis
       -> Decorated AST + Symbol Table -> Intermediate Code -> Interpreter
```

Sesuai QnA Milestone 4, input resmi backend adalah artifact Decorated AST
(DAST). Intermediate Code final adalah instruksi Stack Machine, bukan TAC
eksplisit.

## Build dan CLI

```bash
make
```

Mode input:

```bash
./arion input-artifact.txt [output.txt]
./arion --source input-arion.txt [output.txt]
./arion --parse-tree input-parse-tree.txt [output.txt]
```

- Tanpa flag, input wajib memuat section Decorated AST, Symbol Table, dan
  Semantic Diagnostics.
- `--source` menjalankan seluruh pipeline dari source Arion.
- `--parse-tree` memulai pipeline dari parse tree ter-render.
- Semantic error menghentikan code generation dan menghasilkan exit code
  nonzero.
- Section Intermediate Code dan Program Output dari artifact lama diabaikan
  ketika artifact dibaca ulang.

## Format Artifact

```text
----- Decorated AST -----
...
----- Symbol Table -----
...
----- Semantic Diagnostics -----
...
```

Symbol table memakai `tab`, `btab`, dan `atab`. Kolom `ref` menyimpan referensi
blok/callable, sedangkan `tref` menyimpan referensi tipe sehingga function yang
mengembalikan array atau record tetap dapat direkonstruksi. Parser juga menerima
artifact lama yang belum memiliki kolom `tref`.

Sebelum code generation, `normalizeRuntimeLayout` menghitung ulang:

- alamat variabel dan parameter;
- offset field record;
- ukuran elemen dan total array;
- ukuran parameter dan variabel setiap blok.

Scalar, real, char, boolean, string, subrange, dan enum memakai satu slot.
Ukuran array dan record dihitung secara rekursif.

## Stack Machine

Instruksi wajib:

```text
LIT LOD STO CAL INT JMP JPC OPR RET
```

Extension untuk l-value dan composite:

```text
LDA  load address
LDI  indirect scalar load
STI  indirect scalar store
BLD  block load
BST  block store
ADI  add record-field offset
IXA  checked array-index address
```

`LOD`, `STO`, `LDA`, dan `CAL` menggunakan lexical-level difference. Bentuk call
dan return adalah:

```text
CAL level target argumentSlotCount
RET 0 resultOffset resultSlotCount
```

Operand tambahan disimpan sebagai data instruksi. Komentar hanya dipakai untuk
keterbacaan dan tidak memengaruhi eksekusi. Renderer dan parser Intermediate
Code diuji secara round-trip.

Setiap activation frame memiliki tiga slot logis awal:

```text
0 static link
1 dynamic link
2 return address
3... parameter, function result, dan variabel lokal
```

Metadata frame disimpan terpisah dari slot program. Operand expression juga
memakai stack terpisah, sehingga temporary caller tetap utuh saat function
return. Static link dipakai untuk akses nonlocal; dynamic link hanya dipakai
untuk kembali ke caller.

## Fitur Bahasa

- scalar integer, real, boolean, char, dan string;
- arithmetic, integer division, modulo, relational, dan boolean operation;
- assignment, `if`, `case`, `while`, `repeat`, `for to/downto`;
- `readln`, multi-argument `write`, dan `writeln`;
- procedure/function, zero-argument call, nested scope, dan recursion;
- function call di tengah expression;
- array, record, nested composite, dynamic index, composite assignment;
- parameter dan return value composite;
- definite-assignment analysis pada sequence, branch, dan loop.

## Runtime Safety

Interpreter menghentikan program secara terkontrol dengan exit code nonzero
untuk:

- activation-frame overflow (maksimal 1000 frame);
- operand/frame underflow;
- akses atau penulisan header frame;
- call/return dan operand-depth corruption;
- invalid jump/call target;
- array out-of-bounds;
- division/modulo by zero;
- invalid runtime operand type;
- integer dan real overflow/underflow.

Pesan runtime menyertakan instruction pointer. Error khusus tersedia untuk
bounds, numeric overflow, numeric underflow, invalid memory, dan stack
corruption.

## Pengujian

```bash
make test
make test-sanitize
```

`make test` menjalankan regression M1-M3, golden integration test M4, DAST
round-trip, nested call/recursion, control flow, I/O, array/record, composite
parameter/return, semantic gating, dan unit test vulnerability VM.

`make test-sanitize` membangun dan menjalankan suite yang sama dengan
AddressSanitizer dan UndefinedBehaviorSanitizer.

Keterbatasan saat ini: `readln` membaca token yang dipisahkan whitespace, dan
integer runtime menggunakan signed 32-bit dengan overflow checking.
