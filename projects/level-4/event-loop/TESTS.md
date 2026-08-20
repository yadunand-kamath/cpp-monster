# P-4.1 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(EventLoop, SingleTimerFiresAfterDeadline) {
    EventLoop loop;
    bool fired = false;
    auto start = std::chrono::steady_clock::now();
    loop.schedule_timer(50ms, [&] { fired = true; loop.stop(); });
    loop.run();
    EXPECT_TRUE(fired);
    EXPECT_GE(std::chrono::steady_clock::now() - start, 50ms);
}

TEST(EventLoop, MultipleTimersFireInDeadlineOrder) {
    EventLoop loop;
    std::vector<int> order;
    loop.schedule_timer(30ms, [&] { order.push_back(2); });
    loop.schedule_timer(10ms, [&] { order.push_back(1); });
    loop.schedule_timer(50ms, [&] { order.push_back(3); loop.stop(); });
    loop.run();
    EXPECT_THAT(order, ::testing::ElementsAre(1, 2, 3));
}

TEST(EventLoop, SameDeadlineTimersFireInRegistrationOrder) {
    EventLoop loop;
    std::vector<int> order;
    auto deadline = 20ms;
    loop.schedule_timer(deadline, [&] { order.push_back(1); });
    loop.schedule_timer(deadline, [&] { order.push_back(2); loop.stop(); });
    loop.run();
    EXPECT_THAT(order, ::testing::ElementsAre(1, 2));
}

TEST(EventLoop, SocketReadabilityTriggersCallback) {
    auto [server, client] = make_loopback_tcp_pair();
    EventLoop loop;
    std::string received;
    loop.watch_readable(server, [&] {
        char buf[64]; auto n = recv(server, buf, sizeof(buf), 0);
        received.assign(buf, n);
        loop.stop();
    });
    send(client, "hello", 5, 0);
    loop.run();
    EXPECT_EQ(received, "hello");
}

TEST(EventLoop, CancellingTimerBeforeItFiresPreventsCallback) {
    EventLoop loop;
    bool fired = false;
    auto handle = loop.schedule_timer(100ms, [&] { fired = true; });
    loop.cancel(handle);
    loop.schedule_timer(150ms, [&] { loop.stop(); });
    loop.run();
    EXPECT_FALSE(fired);
}

TEST(EventLoop, CallbackCanRegisterNewTimerFromWithinItself) {
    EventLoop loop;
    int count = 0;
    std::function<void()> tick = [&] {
        if (++count < 3) loop.schedule_timer(10ms, tick);
        else loop.stop();
    };
    loop.schedule_timer(10ms, tick);
    loop.run();
    EXPECT_EQ(count, 3);
}

TEST(EventLoop, CallbackCanCancelAnotherRegistrationFromWithinItself) {
    EventLoop loop;
    bool second_fired = false;
    auto second = loop.schedule_timer(50ms, [&] { second_fired = true; });
    loop.schedule_timer(10ms, [&] { loop.cancel(second); loop.schedule_timer(60ms, [&]{ loop.stop(); }); });
    loop.run();
    EXPECT_FALSE(second_fired);
}

TEST(EventLoop, IdleLoopDoesNotBusyPoll) {
    EventLoop loop;
    loop.schedule_timer(200ms, [&] { loop.stop(); });
    auto cpu_before = read_process_cpu_time();
    loop.run();
    auto cpu_used = read_process_cpu_time() - cpu_before;
    EXPECT_LT(cpu_used, 20ms); // wall time was 200ms; CPU time should be negligible
}
```

## Hidden Tests

- a long-running stress test issuing tens of thousands of timer/I/O registrations and cancellations, checking for fd/handle leaks (via a platform-appropriate handle-count check) and unbounded memory growth
- the Windows-specific overlapped-operation-already-completed-but-not-dequeued cancellation race, exercised deliberately (e.g. via a tight timing window) and checked against the documented observed behavior
- registering an already-closed fd, confirmed to produce a clear immediate error rather than corrupting loop state
- an `epoll_wait`/`GetQueuedCompletionStatus`-level error injection (where feasible via a test seam), confirming the loop reports it rather than silently terminating
- a multi-connection scenario (a listening socket plus several simultaneous established connections) confirming correct interleaving of accept and read/write events on the single thread
