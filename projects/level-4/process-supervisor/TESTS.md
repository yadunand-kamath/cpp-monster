# P-4.6 — Tests

## Visible Tests (GoogleTest)

Assumes a helper executable `test_child` built alongside the test suite, controllable via arguments: `test_child --exit-code N`, `test_child --ignore-signals --loop`, `test_child --burst-output BYTES`, `test_child --sleep-ms N`.

```cpp
TEST(ProcessSupervisor, LaunchesChildAndCapturesStdout) {
    Supervisor sup(SupervisorConfig{});
    sup.start({"test_child", "--print", "hello", "--exit-code", "0"});
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_NE(sup.captured_stdout().find("hello"), std::string::npos);
}

TEST(ProcessSupervisor, NormalExitZeroDoesNotRestartByDefault) {
    Supervisor sup(SupervisorConfig{});
    sup.start({"test_child", "--exit-code", "0"});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(sup.history().restart_count, 0u);
    EXPECT_EQ(sup.state(), SupervisorState::kNotRunning);
}

TEST(ProcessSupervisor, CrashingChildIsRestarted) {
    Supervisor sup(SupervisorConfig{.base_backoff = std::chrono::milliseconds(10)});
    sup.start({"test_child", "--exit-code", "1"});
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_GT(sup.history().restart_count, 0u);
}

TEST(ProcessSupervisor, BackoffDelayIncreasesBetweenAttempts) {
    Supervisor sup(SupervisorConfig{.base_backoff = std::chrono::milliseconds(20), .max_backoff = std::chrono::seconds(5)});
    sup.start({"test_child", "--exit-code", "1"});
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    auto delays = sup.history().observed_restart_delays;
    ASSERT_GE(delays.size(), 3u);
    EXPECT_LT(delays[0], delays[1]);
    EXPECT_LT(delays[1], delays[2]);
}

TEST(ProcessSupervisor, BackoffDelayIsCappedAtMax) {
    Supervisor sup(SupervisorConfig{.base_backoff = std::chrono::milliseconds(10), .max_backoff = std::chrono::milliseconds(100)});
    sup.start({"test_child", "--exit-code", "1"});
    std::this_thread::sleep_for(std::chrono::seconds(2));
    for (auto delay : sup.history().observed_restart_delays) {
        EXPECT_LE(delay, std::chrono::milliseconds(150)); // capped, with small scheduling slack
    }
}

TEST(ProcessSupervisor, GracefulStopOnCooperativeChildAvoidsForcefulKill) {
    Supervisor sup(SupervisorConfig{.grace_period = std::chrono::milliseconds(500)});
    sup.start({"test_child", "--sleep-ms", "10000"});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sup.stop_gracefully();
    EXPECT_TRUE(sup.wait_for_stop(std::chrono::seconds(1)));
    EXPECT_FALSE(sup.history().last_stop_was_forceful);
}

TEST(ProcessSupervisor, GracefulStopOnUncooperativeChildEscalatesToForceful) {
    Supervisor sup(SupervisorConfig{.grace_period = std::chrono::milliseconds(200)});
    sup.start({"test_child", "--ignore-signals", "--loop"});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sup.stop_gracefully();
    EXPECT_TRUE(sup.wait_for_stop(std::chrono::seconds(2)));
    EXPECT_TRUE(sup.history().last_stop_was_forceful);
}

TEST(ProcessSupervisor, DeliberateStopDoesNotTriggerRestart) {
    Supervisor sup(SupervisorConfig{});
    sup.start({"test_child", "--sleep-ms", "10000"});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sup.stop_gracefully();
    sup.wait_for_stop(std::chrono::seconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_EQ(sup.history().restart_count, 0u);
}

TEST(ProcessSupervisor, NonexistentExecutableIsDistinguishableLaunchFailure) {
    Supervisor sup(SupervisorConfig{});
    auto result = sup.start({"this_executable_does_not_exist_xyz"});
    EXPECT_FALSE(result.has_value());
}

TEST(ProcessSupervisor, LargeOutputBurstDoesNotDeadlock) {
    Supervisor sup(SupervisorConfig{});
    sup.start({"test_child", "--burst-output", "5000000", "--exit-code", "0"}); // 5MB, exceeds typical pipe buffer
    ASSERT_TRUE(sup.wait_for_stop(std::chrono::seconds(10))); // would hang forever if deadlocked
}
```

## Hidden Tests

- a repeated-rapid-crash-loop test (child exits in under a millisecond every time) confirming backoff still meaningfully engages and the supervisor doesn't spin at effectively unbounded relaunch frequency
- a race test: sending a graceful-stop request at nearly the same instant the child exits on its own, confirming no crash/unhandled platform error
- a shutdown-during-backoff-wait test confirming the backoff sleep is interruptible by an explicit supervisor shutdown request, not a plain blocking sleep
- a max-restart-attempts-configured test confirming the supervisor stops attempting restarts once the configured cap is reached, and reports a distinguishable terminal state
- a platform-specific test (compiled per-platform) verifying the actual signal/event used for graceful termination (SIGTERM handling on Linux via a helper that logs received signals; CTRL_BREAK_EVENT handling on Windows via an equivalent helper)
