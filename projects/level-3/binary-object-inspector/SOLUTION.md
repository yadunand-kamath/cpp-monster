# P-3.1 — Solution

## Reference Architecture

A shared `MappedFile` (reused from [P-2.1](../../level-2/log-line-indexer/STATEMENT.md)), a bounds-checked "read struct at offset" helper used by both format parsers, and two independent parser classes (`ElfFile`, `PeFile`) behind a common `detect_format`-driven dispatch.

```cpp
template <typename Header>
const Header* read_header_at(std::string_view data, std::size_t offset) {
    if (offset + sizeof(Header) > data.size())
        throw std::runtime_error("header read exceeds file bounds");
    // avoid casting a possibly-misaligned pointer directly:
    static thread_local Header storage;
    std::memcpy(&storage, data.data() + offset, sizeof(Header));
    return &storage;
}

enum class BinaryFormat { Elf, Pe, Unknown };

BinaryFormat detect_format(std::string_view data) {
    if (data.size() >= 4 && data[0] == 0x7f && data[1] == 'E' && data[2] == 'L' && data[3] == 'F')
        return BinaryFormat::Elf;
    if (data.size() >= 2 && data[0] == 'M' && data[1] == 'Z')
        return BinaryFormat::Pe;
    throw std::runtime_error("not a recognized ELF or PE file");
}
```

```cpp
class ElfFile {
public:
    explicit ElfFile(const std::filesystem::path& p) : file_(p) {
        auto data = file_.view();
        auto* eh = read_header_at<Elf64_Ehdr>(data, 0);
        if (eh->e_shoff + eh->e_shnum * eh->e_shentsize > data.size())
            throw std::runtime_error("section header table exceeds file bounds");
        // find the section-name string table section first (e_shstrndx), then
        // walk e_shnum entries starting at e_shoff, resolving sh_name through it
    }
    std::vector<Section> sections() const;
    std::vector<Symbol> symbols() const;      // from .symtab, empty if stripped
    std::vector<Symbol> dynamic_symbols() const; // from .dynsym
private:
    MappedFile file_;
};
```

`PeFile` mirrors this: read the DOS header, validate and follow `e_lfanew` to the COFF/optional headers, validate the section table's offset+count against the file size before walking it, then locate the import/export directories via the optional header's data-directory array (itself bounds-checked the same way).

## Design Rationale

**Why `memcpy` into a local struct instead of `reinterpret_cast`ing a pointer directly into the mapped region?** On-disk ELF/PE structures are packed to match the file format's specification exactly, which does not guarantee the natural alignment C++ expects for direct pointer access to multi-byte fields (a `reinterpret_cast<const uint32_t*>` pointing at an odd file offset is undefined behavior to dereference on some platforms/optimization levels, even if it "usually works" on x86). Copying into a locally-declared, naturally-aligned struct sidesteps this risk entirely, at the cost of a small, fixed-size copy per header read — a correctness-over-micro-performance trade-off appropriate for a tool that reads a bounded number of headers, not per-byte hot-path data.

**Why validate offset-plus-size against the mapped region's total size before every read, rather than trusting the header fields?** This is the entire mechanism by which the project satisfies its "detected, not UB" requirement for corrupted/truncated files — a header field claiming an offset or count that would read past the end of the actual file is exactly the attack/corruption surface this check exists to close. Because both formats work by chasing a sequence of offsets found in earlier headers, a single missed validation anywhere in that chain reopens the vulnerability the rest of the checks were trying to close — consistency across every offset-following step matters more than any individual check's sophistication.

**Why look up existing demangling APIs (POSIX `abi::__cxa_demangle`, or MSVC's `UnDecorateSymbolName`) rather than writing one?** Mangling schemes (Itanium ABI, MSVC's decoration scheme) are large, precisely specified, and have many edge cases (templates, operator overloads, `noexcept`, ref-qualifiers, ABI tags) that a from-scratch demangler would need years of incremental bug-fixing to match correctly — exactly the kind of "not the point of this exercise" scope this project's Functional Requirements explicitly carve out, in contrast to the actual binary-format parsing, which is the skill being exercised.

## Reference Implementation

The above covers the shared bounds-checked reading primitive and the ELF parser's skeleton. Remaining substantial work (appropriately left to the learner, per this project's L effort estimate):
1. `PeFile`'s full header chain (DOS → COFF → optional → section table → data directories for imports/exports).
2. Section-name string-table resolution for ELF (`sh_name` is an index into the section pointed to by `e_shstrndx`, itself just another section requiring the same bounds-checked access).
3. Import/export directory parsing for PE, which involves following several more layers of RVA-based (relative virtual address, not file offset — a distinct concept requiring section-table-based translation) indirection.
4. Wiring the platform demangling API behind one function, called uniformly regardless of which binary format produced the symbol name.

## Testing Strategy

Prefer real compiler-produced binaries over hand-crafted synthetic ones for the positive-case tests — hand-crafted files risk only exercising the shape you assumed while writing the parser, whereas real binaries from your actual toolchain will surface the format's genuine variability (section ordering, padding, optional fields present or absent) that a synthetic file wouldn't necessarily include. Reserve hand-crafted files specifically for the corruption/truncation negative tests, where you need precise control over exactly which byte to corrupt.

## Performance Analysis

Parsing cost is dominated by however many sections/symbols the binary contains — each is a small, fixed-size structure read, so the whole parse is O(section count + symbol count), not O(file size), since memory-mapping means the tool never reads bytes it doesn't need to inspect (large `.text`/`.data` section *contents* are never touched, only their headers).

## Failure Modes

- A missing bounds check on any single offset-following step, reopening an out-of-bounds read for a maliciously or accidentally corrupted file even though other steps are checked correctly.
- RVA-vs-file-offset confusion in the PE import/export parsing — a very common real-world bug source, since PE deliberately distinguishes "where this lives once loaded into memory" (RVA) from "where this lives in the file on disk" (file offset), and the two are not interchangeable without consulting the section table's mapping between them.
- Assuming a specific compiler's typical output shape (e.g. always having a `.symtab`, or exports always being present) rather than handling the format's full legitimate range of variation.

## Extensions

- DWARF debug-info parsing (line-number-to-address mapping) for ELF, and PDB parsing for PE — natural, much larger follow-ups that directly feed into [C-3](../../capstones/systems-profiler-trace-viewer/STATEMENT.md)'s profiler/trace-viewer capstone.
- A `--diff` mode comparing two binaries' section/symbol tables, useful for detecting unintended ABI changes between builds.
