// Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
// Process-local fault injection: reject/coerce connection attributes before any driver connection.
#include <sql.h>
#include <sqlext.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

extern "C" SQLRETURN SQL_API SQLSetConnectAttrW(SQLHDBC dbc, SQLINTEGER attr, SQLPOINTER value,
        SQLINTEGER length) {
    const char* target = std::getenv("ODBC_TEST_ATTRIBUTE");
    if (target && attr == std::atoi(target)) {
        std::fprintf(stderr, "ATTRIBUTE %llu\n", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(value)));
        return std::getenv("ODBC_TEST_INFO") ? SQL_SUCCESS_WITH_INFO : SQL_ERROR;
    }
    using Set = decltype(&SQLSetConnectAttrW);
    static Set real = reinterpret_cast<Set>(dlsym(RTLD_NEXT, "SQLSetConnectAttrW"));
    if (!real) {
        std::abort();
    }
    return real(dbc, attr, value, length);
}

extern "C" SQLRETURN SQL_API SQLDriverConnectA(SQLHDBC, SQLHWND, SQLCHAR*, SQLSMALLINT,
        SQLCHAR*, SQLSMALLINT, SQLSMALLINT*, SQLUSMALLINT) {
    std::fprintf(stderr, "CONNECT\n");
    return SQL_ERROR;
}
