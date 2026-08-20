# P-3.1 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch08's compilation/ABI material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Both ELF and PE share a common shape even though their specific byte layouts differ completely: a fixed-position header at the start of the file tells you where to find a table of section/segment descriptors, which in turn tell you where — and how large — each section's actual data is elsewhere in the file. Think about how you'd represent "read a specific header structure at a specific file offset" in a way that's reusable across both formats' many header structures, given that you already have the whole file available as a contiguous mapped byte range.

## Hint 2 — Technique

For reading a fixed-layout header structure out of a raw byte buffer at a known offset, consider a small helper that reinterprets a pointer into your mapped region as a pointer to your header struct — but validate the offset plus the struct's size against the mapped region's total size *before* doing so, every single time, since this is exactly the boundary where a corrupted-file input could otherwise cause an out-of-bounds read. Also be careful about alignment: some on-disk structures are not naturally aligned for direct pointer-casting on every platform, so consider `memcpy`-ing into a locally-aligned struct instance rather than casting a possibly-misaligned pointer directly. For demangling, look into what demangling functionality your toolchain's own C++ runtime or standard library already exposes before reaching for a separate third-party library.

## Hint 3 — Implementation

For ELF specifically: the ELF header tells you the offset and entry size of the section header table; each section header entry tells you that section's name as an *index into a separate string-table section* — not inline text — so you need to chase that indirection correctly. For PE: the DOS header's `e_lfanew` field tells you where the actual PE/COFF header begins (the DOS header itself is largely vestigial in modern PE files) — validate this field rather than trusting it blindly, since a corrupted or non-PE file could have arbitrary garbage there that would otherwise send your parser to an unrelated part of the file.

## Hint 4 — Debugging/Design

If your tool crashes or produces garbage on a real compiler-produced binary (rather than your own test file), the most likely cause is an assumption baked in from testing against too narrow a sample — e.g., assuming section names are always short, assuming a specific section ordering, or assuming the symbol table is always present. Cross-check your assumptions against the actual format specification (both the ELF specification and the Microsoft PE/COFF specification are publicly available and precise) rather than against the shape of whatever one or two sample files you initially tested with — real-world binaries vary more than a small hand-picked sample suggests, and this is usually where a "works on my test file" implementation breaks first.
