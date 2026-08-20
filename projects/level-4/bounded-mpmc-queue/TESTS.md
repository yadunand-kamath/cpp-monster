# P-4.2 — Tests

## Visible Tests (GoogleTest, parameterized over both variants)

```cpp
template <typename QueueT>
class MpmcQueueTest : public ::testing::Test {};
using QueueTypes = ::testing::Types<LockBasedQueue<int>, LockFreeQueue<int>>;
TYPED_TEST_SUITE(MpmcQueueTest, QueueTypes);

TYPED_TEST(MpmcQueueTest, PushThenPopReturnsSameValue) {
    TypeParam q(16);
    ASSERT_TRUE(q.try_push(42));
    auto v = q.try_pop();
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 42);
}

TYPED_TEST(MpmcQueueTest, TryPushFailsWhenFull) {
    TypeParam q(2);
    ASSERT_TRUE(q.try_push(1));
    ASSERT_TRUE(q.try_push(2));
    EXPECT_FALSE(q.try_push(3));
}

TYPED_TEST(MpmcQueueTest, TryPopFailsWhenEmpty) {
    TypeParam q(4);
    EXPECT_FALSE(q.try_pop().has_value());
}

TYPED_TEST(MpmcQueueTest, CapacityOneWorksUnderConcurrentAccess) {
    TypeParam q(1);
    std::atomic<int> produced = 0, consumed_sum = 0;
    std::vector<std::thread> producers, consumers;
    for (int i = 0; i < 4; ++i) producers.emplace_back([&] {
        for (int j = 0; j < 1000; ++j) { while (!q.try_push(1)) {} produced++; }
    });
    for (int i = 0; i < 4; ++i) consumers.emplace_back([&] {
        for (int j = 0; j < 1000; ++j) {
            std::optional<int> v;
            while (!(v = q.try_pop())) {}
            consumed_sum += *v;
        }
    });
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
    EXPECT_EQ(produced.load(), 4000);
    EXPECT_EQ(consumed_sum.load(), 4000);
}

TYPED_TEST(MpmcQueueTest, StressTestNoLostOrDuplicatedItems) {
    constexpr int kProducers = 8, kPerProducer = 10000;
    TypeParam q(64);
    std::atomic<long long> checksum_pushed = 0, checksum_popped = 0;
    std::atomic<int> popped_count = 0;
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) producers.emplace_back([&, p] {
        for (int i = 0; i < kPerProducer; ++i) {
            int v = p * kPerProducer + i;
            checksum_pushed += v;
            while (!q.try_push(v)) {}
        }
    });
    std::vector<std::thread> consumers;
    for (int c = 0; c < 8; ++c) consumers.emplace_back([&] {
        while (popped_count.load() < kProducers * kPerProducer) {
            if (auto v = q.try_pop()) { checksum_popped += *v; popped_count++; }
        }
    });
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
    EXPECT_EQ(checksum_pushed.load(), checksum_popped.load());
}

TYPED_TEST(MpmcQueueTest, CloseWakesBlockedProducerAndConsumer) {
    TypeParam q(1);
    q.push(1); // fill it
    std::thread producer([&] { EXPECT_FALSE(q.push(2)); }); // blocks, then wakes on close
    std::thread consumer_drain([&] { q.pop(); }); // drains the one item
    std::this_thread::sleep_for(50ms);
    q.close();
    producer.join();
    consumer_drain.join();
}

TYPED_TEST(MpmcQueueTest, CloseWithItemsRemainingStillAllowsDraining) {
    TypeParam q(4);
    q.try_push(1); q.try_push(2);
    q.close();
    EXPECT_EQ(q.try_pop(), std::optional<int>(1));
    EXPECT_EQ(q.try_pop(), std::optional<int>(2));
    EXPECT_EQ(q.try_pop(), std::nullopt); // now genuinely empty AND closed
}

TYPED_TEST(MpmcQueueTest, PushOnClosedQueueFailsClearly) {
    TypeParam q(4);
    q.close();
    EXPECT_FALSE(q.try_push(1));
}
```

## Hidden Tests

- a ThreadSanitizer-instrumented run of the full stress test for the lock-free variant specifically, required to report zero races
- an empty/full boundary-transition-targeted stress test (many threads repeatedly driving the queue to exactly empty and exactly full) checking for lost or duplicated items specifically at that boundary
- a benchmark run at two contention profiles (low: 2p/2c, high: 8p/8c) with throughput numbers captured for both variants
- a per-producer FIFO-ordering check: values from a single producer thread, tagged with a per-producer monotonic sequence number, must be observed by consumers in that producer's original order
- a memory-ordering-downgrade experiment: deliberately weakening one atomic operation's ordering (e.g. acquire→relaxed) and confirming ThreadSanitizer or the correctness stress test now fails, proving the originally-chosen ordering was load-bearing and not overly conservative
