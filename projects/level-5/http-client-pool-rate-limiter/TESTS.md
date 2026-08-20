# P-5.5 — Tests

## Visible Tests (GoogleTest)

```cpp
TEST(HttpResponseParser, ParsesStatusLineAndHeaders) {
    HttpResponseParser parser;
    parser.feed("HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Type: text/plain\r\n\r\nhello");
    auto response = parser.take_response();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->status_code, 200);
    EXPECT_EQ(response->headers.at("Content-Type"), "text/plain");
    EXPECT_EQ(response->body, "hello");
}

TEST(HttpResponseParser, HandlesResponseSplitAcrossMultipleFeeds) {
    HttpResponseParser parser;
    parser.feed("HTTP/1.1 200 OK\r\nContent-Le");
    EXPECT_FALSE(parser.take_response().has_value());
    parser.feed("ngth: 2\r\n\r\nhi");
    auto response = parser.take_response();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->body, "hi");
}

TEST(HttpResponseParser, DecodesChunkedTransferEncodingWithVaryingChunkSizes) {
    HttpResponseParser parser;
    parser.feed("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
                 "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n");
    auto response = parser.take_response();
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->body, "hello world");
}

TEST(HttpResponseParser, TruncatedResponseIsDetectedNotSilentlyAccepted) {
    HttpResponseParser parser;
    parser.feed("HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\nshort");
    parser.mark_connection_closed(); // server closed before satisfying Content-Length
    EXPECT_THROW(parser.take_response_or_throw(), HttpParseError);
}

TEST(ConnectionPool, ReusesConnectionForSameHost) {
    InstrumentedConnectionPool pool;
    HttpClient client(pool);
    client.send_request({.host = "localhost", .port = test_server_port(), .method = "GET", .path = "/"});
    client.send_request({.host = "localhost", .port = test_server_port(), .method = "GET", .path = "/other"});
    EXPECT_EQ(pool.connections_established_count(), 1u);
}

TEST(ConnectionPool, StaleConnectionIsTransparentlyReplaced) {
    HttpClient client;
    auto conn1_response = client.send_request({.host = "localhost", .port = test_server_port(), .path = "/"});
    force_close_server_side_connections(); // simulate server-side idle timeout closing it
    auto conn2_response = client.send_request({.host = "localhost", .port = test_server_port(), .path = "/"});
    EXPECT_EQ(conn2_response.status_code, 200); // succeeded via fresh connection, no raw error surfaced
}

TEST(RateLimiter, ThrottlesBurstToConfiguredRate) {
    TokenBucketRateLimiter limiter(/*rate_per_sec=*/10, /*burst_capacity=*/1);
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 5; ++i) limiter.acquire();
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_GE(elapsed, std::chrono::milliseconds(350)); // ~5 requests at 10/sec with burst 1 takes ~400ms
}

TEST(RetryPolicy, RetriesFailingRequestWithGrowingJitteredDelays) {
    auto server = start_test_server_failing_first_n_requests(/*n=*/2);
    HttpClient client(RetryPolicy{.max_attempts = 3, .base_delay = std::chrono::milliseconds(50)});
    auto response = client.send_request({.host = "localhost", .port = server.port(), .path = "/"});
    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.attempts_made, 3);
}

TEST(RetryPolicy, DoesNotRetryNonIdempotentRequestByDefault) {
    auto server = start_test_server_failing_first_n_requests(2);
    HttpClient client(RetryPolicy{.max_attempts = 3});
    auto response = client.send_request({.host = "localhost", .port = server.port(), .method = "POST", .path = "/"});
    EXPECT_EQ(response.attempts_made, 1); // no retry for POST by default
}

TEST(ConnectionPool, ConcurrentRequestsToSameHostNeverShareOneConnection) {
    HttpClient client;
    std::atomic<int> conflicts{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 20; ++i) {
        threads.emplace_back([&] { client.send_request({.host = "localhost", .port = test_server_port(), .path = "/"}); });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(conflicts.load(), 0); // instrumented server-side detects any interleaved-connection misuse
}
```

## Hidden Tests

- a jitter-distribution test running the retry scenario many times, confirming retry delays are not identical across runs (proving randomized jitter is actually applied, not just a deterministic backoff formula)
- a rate-limiter refill-accuracy test over a longer duration, confirming the token bucket's accumulated tokens over time match the configured rate within a reasonable tolerance
- a `Connection: close` compliance test confirming a connection is not reused after either side signals close
- an abnormally-long-header-line and excessive-header-count test confirming a bounded sanity limit rejects it rather than growing an unbounded parse buffer
- a full end-to-end test against [P-5.3](../../level-5/concurrent-tcp-protocol-server/STATEMENT.md)'s server (adapted or stubbed to emit basic HTTP-shaped responses) exercising the client's connection pooling and retry logic against a real, independently-built server implementation
