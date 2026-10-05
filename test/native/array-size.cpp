// Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
#include "ODBCArraySize.h"
#include <cassert>
#include <cstdio>

int main() {
    const size_t maximum = std::numeric_limits<size_t>::max();
    struct Case {
        size_t count;
        size_t width;
        bool valid;
        size_t expected;
    };
    const Case cases[] = {
        {0, 0, true, 1}, {0, maximum, true, 1}, {maximum, 0, true, 1},
        {1, 1, true, 1}, {256, 32, true, 8192}, {maximum, 1, true, maximum},
        {1, maximum, true, maximum}, {maximum / 2, 2, true, maximum - 1},
        {2, maximum / 2, true, maximum - 1}, {maximum / 2 + 1, 2, false, 0},
        {2, maximum / 2 + 1, false, 0}, {maximum, maximum, false, 0},
    };
    for (const Case& test : cases) {
        size_t bytes = 42;
        assert(odbc::getArrayStorageSize(test.count, test.width, bytes) == test.valid);
        assert(bytes == test.expected);
    }
    std::puts("12 array storage boundary cases passed");
}
