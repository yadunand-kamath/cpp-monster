# P-3.7 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(TcpRequestResponse, SingleClientSmallPayloadRoundTrip) {
    TestServer server;  // starts server on an ephemeral port, stops in destructor
    TcpClient client(server.port());
    auto response = client.send_request("hello");
    EXPECT_EQ(response, "HELLO");  // documented transform: uppercase echo
}

TEST(TcpRequestResponse, LargePayloadRequiringMultipleRecvCalls) {
    TestServer server;
    TcpClient client(server.port());
    std::string large_payload(1'000'000, 'x');  // far larger than one recv() buffer
    auto response = client.send_request(large_payload);
    EXPECT_EQ(response.size(), large_payload.size());
}

TEST(TcpRequestResponse, MultipleSequentialRequestsOnOneConnection) {
    TestServer server;
    TcpClient client(server.port());
    EXPECT_EQ(client.send_request("one"), "ONE");
    EXPECT_EQ(client.send_request("two"), "TWO");
    EXPECT_EQ(client.send_request("three"), "THREE");
}

TEST(TcpRequestResponse, TwentyConcurrentClientsNoCrossTalk) {
    TestServer server;
    constexpr int kClients = 20;
    std::vector<std::thread> threads;
    std::vector<bool> ok(kClients, false);
    for (int i = 0; i < kClients; ++i) {
        threads.emplace_back([&, i] {
            TcpClient client(server.port());
            std::string payload = "client-" + std::to_string(i);
            std::string expected = payload;
            for (auto& c : expected) c = std::toupper(c);
            ok[i] = (client.send_request(payload) == expected);
        });
    }
    for (auto& t : threads) t.join();
    EXPECT_TRUE(std::all_of(ok.begin(), ok.end(), [](bool b) { return b; }));
}

TEST(TcpRequestResponse, TruncatedMessageMidPayloadDetected) {
    TestServer server;
    RawSocket sock;
    sock.connect(server.port());
    sock.send_raw(encode_length_prefix(100));  // claims 100 bytes coming
    sock.send_raw("only 10 b");                 // sends far fewer, then...
    sock.close();                                // ...disconnects mid-frame
    // Server must detect this as a closed/truncated connection, not hang or
    // misinterpret partial bytes as a complete (wrong-length) message.
    EXPECT_TRUE(server.observed_truncated_connection_within(2s));
}

TEST(TcpRequestResponse, OversizedLengthPrefixRejected) {
    TestServer server;
    RawSocket sock;
    sock.connect(server.port());
    sock.send_raw(encode_length_prefix(0xFFFFFFFF));  // implausibly large
    EXPECT_TRUE(server.connection_closed_within(2s));
    EXPECT_FALSE(server.attempted_unbounded_allocation());  // instrumented counter
}

TEST(TcpRequestResponse, ConnectToNonListeningPortReportsClearError) {
    // No server started on this port.
    EXPECT_THROW(TcpClient client(get_unused_port()), ConnectionError);
}

TEST(TcpRequestResponse, CleanShutdownWhileConnectionsActive) {
    TestServer server;
    TcpClient client(server.port());
    std::thread t([&] { client.send_request("in-flight"); });
    server.stop();  // must not hang, must not silently drop without documented close
    t.join();
}
```

## Hidden Tests

- A stress variant of the concurrent-clients test with 100+ simultaneous connections, checking the server degrades (documented thread-count ceiling) rather than crashing or corrupting other connections' data.
- A payload of exactly one byte and a payload of exactly zero bytes (empty message body, valid per the framing format) — boundary sizes often missed by tests that only try "small" and "large."
- A client that sends the length prefix one byte at a time with deliberate delays between each byte (simulating a slow network), confirming the server's `recv_framed` loop correctly accumulates rather than misreading a partially-arrived length prefix.
- Sending two complete frames back-to-back in a single `send()` call from the client, confirming the server correctly splits them into two separate requests rather than concatenating or dropping data (this is the same reassembly problem P-5.3 confronts at scale — worth catching here first at a scale small enough to debug by hand).
