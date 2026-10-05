// Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
#include <sql.h>
#include <sqlext.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
extern "C" SQLRETURN SQL_API SQLBindParameter(SQLHSTMT stmt, SQLUSMALLINT column,
        SQLSMALLINT direction, SQLSMALLINT ctype, SQLSMALLINT sqltype, SQLULEN size,
        SQLSMALLINT digits, SQLPOINTER data, SQLLEN buflen, SQLLEN* indicator) {
    using Bind = decltype(&SQLBindParameter);
    static Bind real = reinterpret_cast<Bind>(dlsym(RTLD_NEXT, "SQLBindParameter"));
    static bool injected = false;
    if (!real) { std::abort(); }
    if (!injected && column == 1 && indicator && *indicator == SQL_NULL_DATA) {
        injected = true;
        std::fprintf(stderr, "ODBC_BIND_TRACE injected column 1\n");
        return SQL_ERROR;
    }
    if (injected) { std::fprintf(stderr, "ODBC_BIND_TRACE bind column %u\n", column); }
    return real(stmt, column, direction, ctype, sqltype, size, digits, data, buflen, indicator);
}
