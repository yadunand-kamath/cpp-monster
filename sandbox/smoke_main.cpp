// Proof-of-life executable for the build harness. Not part of the
// curriculum — see sandbox/CMakeLists.txt.
#include <cstdio>

int add(int a, int b) { return a + b; }

int main() {
    std::printf("cpp-workbook build harness OK: 2 + 2 = %d\n", add(2, 2));
    return 0;
}
