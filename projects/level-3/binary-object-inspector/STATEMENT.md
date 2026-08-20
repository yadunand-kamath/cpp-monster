# P-3.1 — Binary Object File Inspector (ELF + PE)

**Level:** 3 (Realistic utility) · **Category:** Dev Tools · **Requires:** Ch01–09 · **Est. effort:** L (16-24h)

## Objective

Build a command-line tool that parses both ELF (Linux) and PE (Windows) object/executable files directly from their binary format — no calling out to `objdump`/`dumpbin` — and reports sections, symbols, and imports/exports, with C++ symbol names demangled into human-readable form.

## Functional Requirements

1. Parse ELF files (at minimum, 64-bit little-endian, the common case on modern Linux) directly from the file header through the section header table, reporting each section's name, type, size, and file offset.
2. Parse PE files (at minimum, PE32+ for 64-bit) directly from the DOS header through the COFF header through the optional header through the section table, reporting the same category of information.
3. Extract and list the symbol table for each format: for ELF, the `.symtab`/`.dynsym` entries; for PE, exported symbols (from the export directory) and imported symbols (from the import directory), including which DLL each import comes from.
4. Demangle C++ symbol names (Itanium ABI mangling for ELF/GCC-Clang-produced binaries, MSVC's own mangling scheme for PE/MSVC-produced binaries) into human-readable form — you may use a library for this specifically (writing a full demangler is its own multi-week project and is explicitly out of scope; document which demangling library or API you used and why).
5. Detect which format a given file is (by inspecting its magic bytes) and dispatch to the correct parser automatically — the tool should not require the user to specify the format manually.
6. Handle both formats' variable-length, offset-based structures correctly — meaning your parser reads header fields to determine where subsequent structures live in the file, rather than assuming fixed offsets that happen to work for one specific compiler's output.

## Input

A path to a binary file (ELF object/executable/shared-library, or PE object/executable/DLL).

## Output

A structured report (plain text or a simple structured format like a table) listing: detected format, sections with their properties, symbols (demangled where applicable), and for PE specifically, the import/export tables with DLL names.

## Constraints

- C++20. No dependency on `objdump`, `dumpbin`, `nm`, or any other external binary-inspection tool at runtime — your tool must parse the binary format itself.
- Must not assume the input file fits comfortably in memory as a single read for arbitrarily large binaries — memory-mapping (as in [P-2.1](../../level-2/log-line-indexer/STATEMENT.md)) is the natural approach here too, and reusing/adapting that project's mapping wrapper is explicitly encouraged rather than discouraged.
- Must correctly reject (with a clear error, not a crash) a file that is not a valid ELF or PE file at all (wrong magic bytes).

## Edge Cases

- A stripped binary (no symbol table) — must report "no symbols" cleanly, not crash trying to parse a missing/empty section.
- A PE file with no imports (a fully self-contained executable) or no exports (a typical `.exe` rather than a `.dll`) — both tables being legitimately empty.
- An ELF file with a `.dynsym` but no `.symtab` (common for stripped shared libraries relying only on dynamic symbols) — versus one with both.
- A truncated or corrupted file (valid magic bytes, but headers claiming offsets/sizes that exceed the actual file size) — must be detected and reported, not read past the mapped region.

## Error Handling

- Invalid magic bytes — a clear "not a recognized ELF or PE file" error.
- Truncated/corrupted headers (offset or size fields pointing outside the actual file) — a clear parse error, never an out-of-bounds read of the mapped file.
- A recognized-but-unsupported format variant (e.g. 32-bit ELF, if you chose to support only 64-bit) — a clear "unsupported variant" message distinct from "not a valid file at all."

## Acceptance Criteria

- Correctly parses and reports on at least: one ELF executable, one ELF shared library, one PE executable (`.exe`), and one PE DLL — real files produced by your own toolchain's compiler (MSVC for PE, WSL Clang/GCC for ELF) are the expected test corpus, not synthetic hand-crafted files.
- Demangled C++ symbol names from a test binary containing overloaded functions, templates, and namespaced symbols are shown correctly (cross-checked against `nm -C` or `dumpbin`'s own demangling as a reference, used only to *verify* your output, never to *produce* it at runtime).
- Correctly detects and rejects at least one non-binary or wrong-magic-bytes input file with a clear error.
- Builds and runs correctly on both MSVC/Windows and WSL Clang/GCC (the tool itself should be able to parse *either* format's files regardless of which platform it's running on — parsing PE files should work fine from Linux, and vice versa, since this is pure binary parsing, not OS API usage).

## Testing Requirements

- Format-detection tests for both magic-byte signatures plus at least one invalid-file rejection test.
- Section/symbol-extraction correctness tests against real compiler-produced binaries, cross-checked against reference tool output for at least a sample of entries (not necessarily exhaustively, given how large real symbol tables can be).
- The stripped-binary and empty-import/export-table edge cases.
- A deliberately truncated/corrupted file test confirming graceful error reporting rather than a crash or out-of-bounds read (run this specific test under ASan).

## Hints

### Hint 1 — Direction
Both ELF and PE share a common shape even though their specific byte layouts differ completely: a fixed-position header at the start of the file that tells you where to find a table of section/segment descriptors, which in turn tell you where (and how large) each section's actual data is elsewhere in the file. Think about how you'd represent "read a specific header structure at a specific file offset" in a way that's reusable across both formats' many header structures, given that you already have the whole file available as a contiguous mapped byte range (per [P-2.1](../../level-2/log-line-indexer/STATEMENT.md)'s mapping technique).

### Hint 2 — Technique
For reading a fixed-layout header structure out of a raw byte buffer at a known offset, consider a small helper that reinterprets (carefully, respecting alignment — some of these on-disk structures are *not* naturally aligned for direct pointer-casting on all platforms) a pointer into your mapped region as a pointer to your header struct — but validate the offset plus the struct's size against the mapped region's total size *before* doing so, every single time, since this is exactly the boundary where a corrupted-file input could otherwise cause an out-of-bounds read. For demangling, look into what demangling functionality your toolchain's own C++ runtime or standard library already exposes (there are POSIX and Windows APIs specifically for this) before reaching for a separate third-party library.

### Hint 3 — Implementation
For ELF specifically: the ELF header tells you the offset and entry size of the section header table; each section header entry tells you that section's name (as an index into a separate string-table section, not inline text!), type, and where its actual data lives. Chase these offsets in the order the format defines rather than guessing typical values. For PE: the DOS header's `e_lfanew` field tells you where the actual PE/COFF header begins (the DOS header itself is largely vestigial in modern PE files, existing only for backward compatibility with old DOS "this program cannot be run in DOS mode" behavior) — don't skip validating this field, since a corrupted or non-PE file could have arbitrary garbage here.

### Hint 4 — Debugging/Design
If your tool crashes or produces garbage on a real compiler-produced binary (rather than your own test file), the most likely cause is an assumption baked in from testing against too narrow a sample — e.g., assuming section names are always short, assuming a specific section ordering, or assuming the symbol table is always present. Cross-check your assumptions against the actual format specification (the ELF specification and the Microsoft PE/COFF specification are both publicly available and precise) rather than against the shape of whatever one or two sample files you initially tested with, since real-world binaries vary more than a small hand-picked sample suggests.
