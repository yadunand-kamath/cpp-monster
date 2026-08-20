# Proof-of-life target for the build harness itself. This has nothing to do
# with the curriculum — it exists so `cmake --preset ... && ctest --preset ...`
# has at least one real, always-passing test to run, so the harness can be
# verified end-to-end before any chapter content depends on it.

add_executable(sandbox_smoke smoke_main.cpp)
target_link_libraries(sandbox_smoke PRIVATE workbook_warnings workbook_sanitizers)

add_executable(sandbox_smoke_tests smoke_test.cpp)
target_link_libraries(sandbox_smoke_tests PRIVATE workbook_warnings workbook_sanitizers GTest::gtest_main)

gtest_discover_tests(sandbox_smoke_tests)
