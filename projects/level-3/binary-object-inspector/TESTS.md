# P-3.1 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(FormatDetection, RecognizesElfMagic) {
    auto path = write_temp_bytes({0x7f, 'E', 'L', 'F', /* ... */});
    EXPECT_EQ(detect_format(path), BinaryFormat::Elf);
}

TEST(FormatDetection, RecognizesPeMagic) {
    auto path = compile_sample_pe_exe(); // real MSVC-produced .exe
    EXPECT_EQ(detect_format(path), BinaryFormat::Pe);
}

TEST(FormatDetection, RejectsInvalidMagicBytes) {
    auto path = write_temp_bytes({'N', 'O', 'T', 'A', 'B', 'I', 'N'});
    EXPECT_THROW(detect_format(path), std::runtime_error);
}

TEST(ElfParser, ListsSectionsFromRealSharedLibrary) {
    auto path = compile_sample_elf_so(); // real WSL-Clang-produced .so
    ElfFile elf(path);
    auto sections = elf.sections();
    EXPECT_TRUE(std::any_of(sections.begin(), sections.end(),
        [](auto& s) { return s.name == ".text"; }));
}

TEST(ElfParser, StrippedBinaryReportsNoSymbolsCleanly) {
    auto path = compile_and_strip_sample_elf();
    ElfFile elf(path);
    EXPECT_TRUE(elf.symbols().empty());
}

TEST(ElfParser, DynsymPresentWithoutSymtabIsHandled) {
    auto path = compile_sample_elf_so_stripped_symtab();
    ElfFile elf(path);
    EXPECT_FALSE(elf.dynamic_symbols().empty());
    EXPECT_TRUE(elf.symbols().empty());
}

TEST(PeParser, ListsSectionsFromRealExecutable) {
    auto path = compile_sample_pe_exe(); // real MSVC-produced .exe
    PeFile pe(path);
    auto sections = pe.sections();
    EXPECT_TRUE(std::any_of(sections.begin(), sections.end(),
        [](auto& s) { return s.name == ".text"; }));
}

TEST(PeParser, ExecutableHasNoExportsButHasImports) {
    auto path = compile_sample_pe_exe();
    PeFile pe(path);
    EXPECT_TRUE(pe.exports().empty());
    EXPECT_FALSE(pe.imports().empty());
}

TEST(PeParser, DllHasExports) {
    auto path = compile_sample_pe_dll();
    PeFile pe(path);
    EXPECT_FALSE(pe.exports().empty());
}

TEST(PeParser, ImportsAreGroupedByDll) {
    auto path = compile_sample_pe_exe();
    PeFile pe(path);
    auto imports = pe.imports();
    EXPECT_TRUE(std::any_of(imports.begin(), imports.end(),
        [](auto& i) { return i.dll_name.find(".dll") != std::string::npos; }));
}

TEST(Demangling, ItaniumOverloadedFunctionsDemangleCorrectly) {
    auto path = compile_elf_with_overloads(); // int foo(int), int foo(double)
    ElfFile elf(path);
    auto syms = elf.symbols();
    EXPECT_TRUE(std::any_of(syms.begin(), syms.end(),
        [](auto& s) { return demangle(s.name) == "foo(int)"; }));
    EXPECT_TRUE(std::any_of(syms.begin(), syms.end(),
        [](auto& s) { return demangle(s.name) == "foo(double)"; }));
}

TEST(Demangling, MsvcTemplateAndNamespacedSymbolsDemangleCorrectly) {
    auto path = compile_pe_with_templates(); // e.g. namespace ns { template<T> struct S {...}; }
    PeFile pe(path);
    auto exports = pe.exports();
    // cross-checked manually against dumpbin's own demangled output
}

TEST(CorruptedFile, TruncatedElfHeaderIsDetected) {
    auto path = write_temp_bytes(truncated_elf_header_bytes());
    EXPECT_THROW(ElfFile(path), std::runtime_error);
}

TEST(CorruptedFile, SectionOffsetBeyondFileSizeIsDetected) {
    auto path = write_corrupted_elf_with_oob_section_offset();
    EXPECT_THROW(ElfFile(path), std::runtime_error);
}
```

## Hidden Tests

- an ASan-instrumented run over the deliberately truncated/corrupted-file tests, confirming no out-of-bounds read of the mapped region occurs even when the parse correctly fails
- a PE object file (`.obj`, not `.exe`/`.dll`) — a related but distinct PE variant with no optional header
- a 32-bit ELF file, if the submission documents 64-bit-only support — checked to be a clear "unsupported variant" error, not a crash or misparse
- cross-platform correctness: parsing a PE file while running the tool on Linux, and parsing an ELF file while running the tool on Windows, confirming the parsers are genuinely platform-independent binary parsing rather than accidentally depending on OS-specific APIs
