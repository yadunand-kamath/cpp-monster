# P-4.4 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(ArenaAllocator, BasicAllocationReturnsUsableMemory) {
    ArenaAllocator arena(4096);
    void* p = arena.allocate(64, alignof(std::max_align_t));
    ASSERT_NE(p, nullptr);
    std::memset(p, 0xAB, 64); // must be safely writable
}

TEST(ArenaAllocator, OverAlignedTypeIsCorrectlyAligned) {
    ArenaAllocator arena(4096);
    void* p = arena.allocate(64, 32);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p) % 32, 0u);
}

TEST(ArenaAllocator, ResetReclaimsAllMemoryInConstantTime) {
    ArenaAllocator arena(1 << 20);
    for (int i = 0; i < 10000; ++i) arena.allocate(64, 8);
    auto start = std::chrono::steady_clock::now();
    arena.reset();
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(elapsed, 1ms);
    void* p = arena.allocate(64, 8); // arena is usable again after reset
    EXPECT_NE(p, nullptr);
}

TEST(ArenaAllocator, GrowingDoesNotInvalidateExistingPointers) {
    ArenaAllocator arena(64); // deliberately tiny, forces growth
    void* first = arena.allocate(32, 8);
    std::memset(first, 0x11, 32);
    for (int i = 0; i < 1000; ++i) arena.allocate(128, 8); // force multiple chunk growths
    EXPECT_EQ(static_cast<unsigned char*>(first)[0], 0x11); // still valid, still correct
}

TEST(PoolAllocator, AllocateDeallocateRoundTrip) {
    PoolAllocator pool(/*block_size=*/64, /*block_align=*/8);
    void* p = pool.allocate();
    ASSERT_NE(p, nullptr);
    pool.deallocate(p);
}

TEST(PoolAllocator, FreedBlockIsReusedNotLeaked) {
    PoolAllocator pool(64, 8);
    void* a = pool.allocate();
    pool.deallocate(a);
    void* b = pool.allocate();
    EXPECT_EQ(a, b); // free-list reuse, not a fresh chunk allocation
}

TEST(PoolAllocator, OverAlignedBlockSizeIsHonored) {
    PoolAllocator pool(64, 32);
    void* p = pool.allocate();
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p) % 32, 0u);
}

TEST(PmrIntegration, VectorUsingArenaResourceWorksCorrectly) {
    ArenaMemoryResource resource(4096);
    std::pmr::vector<int> v(&resource);
    for (int i = 0; i < 100; ++i) v.push_back(i);
    EXPECT_EQ(v.size(), 100u);
    EXPECT_EQ(v[50], 50);
}

TEST(PmrIntegration, MapUsingPoolResourceWorksCorrectly) {
    PoolMemoryResource resource(sizeof(std::pair<const int, int>) + 32, 16);
    std::pmr::map<int, int> m(&resource);
    m[1] = 10; m[2] = 20;
    EXPECT_EQ(m.at(1), 10);
}
```

## Hidden Tests

- an ASan-instrumented run of both allocators' full test suites, confirming clean results (or documented, deliberate hook integration if false positives were observed and addressed)
- an allocation request exceeding the pool's configured block size, checked against the documented reject-or-fallback policy
- a benchmark run comparing arena-reset cost against an equivalent number of individual `delete` calls under the default allocator, with captured timing numbers
- a benchmark run comparing pool-allocator throughput for many same-sized alloc/free cycles against the default allocator, with captured timing numbers and described methodology (warm-up, prevention of optimizer elision)
- a locality-sensitive workload (e.g. traversing a linked structure allocated via the pool vs. via the default allocator) showing a measurable cache-behavior difference, or an honest report if none was observed
