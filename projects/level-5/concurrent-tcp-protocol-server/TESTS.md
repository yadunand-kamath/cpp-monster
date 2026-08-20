# P-5.3 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(FrameParser, ReassemblesMessageSplitAcrossMultipleReads) {
    FrameParser parser(/*max_message_size=*/1 << 20);
    auto msg = encode_frame("hello world");
    parser.feed(msg.substr(0, 3));
    EXPECT_TRUE(parser.next_frame().empty()); // no complete frame yet
    parser.feed(msg.substr(3));
    auto frame = parser.next_frame();
    ASSERT_FALSE(frame.empty());
    EXPECT_EQ(frame, "hello world");
}

TEST(FrameParser, SplitsMultipleMessagesFromSingleRead) {
    FrameParser parser(1 << 20);
    parser.feed(encode_frame("first") + encode_frame("second"));
    EXPECT_EQ(parser.next_frame(), "first");
    EXPECT_EQ(parser.next_frame(), "second");
}

TEST(FrameParser, OversizedLengthPrefixIsRejectedWithoutAllocating) {
    FrameParser parser(/*max_message_size=*/1024);
    std::string oversized_prefix = encode_length_prefix(4ull * 1024 * 1024 * 1024); // 4GB claimed
    EXPECT_THROW(parser.feed(oversized_prefix), FrameProtocolError);
}

TEST(FrameParser, PartialLengthPrefixDoesNotCrashOrMisparse) {
    FrameParser parser(1 << 20);
    parser.feed(std::string(2, '\0')); // only 2 of 4 length-prefix bytes
    EXPECT_TRUE(parser.next_frame().empty());
}

TEST(ConnectionBackpressure, SlowReaderTriggersConfiguredPolicy) {
    Server server(ServerConfig{.max_queued_write_bytes = 4096, .backpressure_policy = BackpressurePolicy::kDisconnect});
    auto client = connect_slow_reading_test_client(server.port());
    for (int i = 0; i < 1000; ++i) server.send_to(client.connection_id(), std::string(1024, 'x'));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(server.connection_was_closed(client.connection_id()));
}

TEST(ConnectionLifecycle, IdleConnectionTimesOutPerPolicy) {
    Server server(ServerConfig{.idle_timeout = std::chrono::milliseconds(100)});
    auto client = connect_test_client(server.port()); // connects, sends nothing
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(server.connection_was_closed(client.connection_id()));
}

TEST(ConnectionLifecycle, PartialFrameNeverCompletedEventuallyTimesOut) {
    Server server(ServerConfig{.partial_frame_timeout = std::chrono::milliseconds(100)});
    auto client = connect_test_client(server.port());
    client.send_raw_bytes(encode_length_prefix(100)); // promises 100 bytes, sends none of them
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(server.connection_was_closed(client.connection_id()));
}

TEST(ServerLifecycle, GracefulShutdownCompletesInFlightRequestsThenCloses) {
    Server server(ServerConfig{});
    auto client = connect_test_client(server.port());
    client.send_request(encode_frame("slow_request"));
    server.shutdown_gracefully();
    EXPECT_TRUE(client.receives_response_within(std::chrono::seconds(2)));
}

TEST(ServerLifecycle, ImmediateShutdownClosesAllConnectionsNow) {
    Server server(ServerConfig{});
    auto client = connect_test_client(server.port());
    server.shutdown_immediately();
    EXPECT_TRUE(client.connection_closed_within(std::chrono::milliseconds(200)));
}

TEST(ServerStartup, BindFailureOnPortInUseIsClearError) {
    Server server1(ServerConfig{.port = 18734});
    EXPECT_THROW(Server(ServerConfig{.port = 18734}), BindError);
}
```

## Manual/Script-Driven Verification

- The 10,000-concurrent-connection scale test, run with documented OS file-descriptor/handle limit configuration, using a load-testing client that opens and maintains that many connections while issuing a representative request rate.
- The load-test client's p50/p99/p999 latency report, with concurrency level, request rate, and duration documented alongside the numbers.

## Hidden Tests

- an abrupt-disconnect (TCP RST) test confirming the server cleans up that connection's resources without affecting any other concurrently active connection
- a maximum-connection-count-reached test confirming the documented policy (reject vs queue) for a connection attempt beyond the configured cap
- a mixed fast-and-slow-client stress test confirming a single slow client's backpressure engagement doesn't degrade throughput for concurrently connected fast clients
- a fuzzed-input test feeding syntactically-invalid frame data (garbage bytes, truncated prefixes, negative-looking lengths if the encoding allows it) confirming no crash across a large number of random malformed inputs
- a sustained-duration soak test (server running under moderate load for an extended period) confirmed via memory-usage tracking to show no unbounded growth (a proxy for connection/resource leak detection)
