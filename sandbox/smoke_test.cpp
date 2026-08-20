// Proof-of-life GoogleTest target for the build harness. Confirms
// FetchContent + gtest_discover_tests() + CTest actually wire together
// end-to-end. Not part of the curriculum — see sandbox/CMakeLists.txt.
#include <gtest/gtest.h>

namespace {

int add_impl(int a, int b) { return a + b; }

TEST(BuildHarnessSmokeTest, AdditionWorks) {
    EXPECT_EQ(add_impl(2, 2), 4);
}

TEST(BuildHarnessSmokeTest, GoogleTestAssertionsRun) {
    EXPECT_TRUE(true);
    ASSERT_NE(1, 2);
}

}  // namespace
