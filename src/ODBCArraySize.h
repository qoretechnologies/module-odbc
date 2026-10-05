// Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
#ifndef QORE_ODBC_ARRAY_SIZE_H
#define QORE_ODBC_ARRAY_SIZE_H

#include <cstddef>
#include <limits>

namespace odbc {

// Zero-width SQL values still require an addressable parameter buffer.
inline bool getArrayStorageSize(size_t count, size_t width, size_t& bytes) {
    bytes = 0;
    if (width && count > std::numeric_limits<size_t>::max() / width) {
        return false;
    }
    bytes = count * width;
    if (!bytes) {
        bytes = 1;
    }
    return true;
}

}
#endif
