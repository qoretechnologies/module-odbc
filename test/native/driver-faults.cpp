// Copyright 2026 Qore Technologies, s.r.o.; SPDX-License-Identifier: MIT
// Driver fault interposer for the ODBC module's native regressions; preloaded only into a test process.
//
// ODBC_FAULT_NEED_DATA=1: a character parameter bound with the value "__need_data__" is bound for data at
//     execution instead (a real SQL_DATA_AT_EXEC indicator, as SQL_LONGVARCHAR, the type that FreeTDS accepts
//     data at execution for), so the driver's execution returns SQL_NEED_DATA
// ODBC_FAULT_DISCONNECT=1: SQLDisconnect() fails without disconnecting, as a driver that cannot disconnect
#include <sql.h>
#include <sqlext.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
bool enabled(const char* name) {
    const char* v = std::getenv(name);
    return v && !std::strcmp(v, "1");
}

const char NeedDataMarker[] = "__need_data__";
SQLLEN dataAtExec = SQL_DATA_AT_EXEC;
}

extern "C" SQLRETURN SQL_API SQLBindParameter(SQLHSTMT stmt, SQLUSMALLINT column,
        SQLSMALLINT direction, SQLSMALLINT ctype, SQLSMALLINT sqltype, SQLULEN size,
        SQLSMALLINT digits, SQLPOINTER data, SQLLEN buflen, SQLLEN* indicator) {
    using Bind = decltype(&SQLBindParameter);
    static Bind real = reinterpret_cast<Bind>(dlsym(RTLD_NEXT, "SQLBindParameter"));
    if (!real) {
        std::abort();
    }
    if (enabled("ODBC_FAULT_NEED_DATA") && ctype == SQL_C_CHAR && data && indicator
            && *indicator == static_cast<SQLLEN>(sizeof(NeedDataMarker) - 1)
            && !std::memcmp(data, NeedDataMarker, sizeof(NeedDataMarker) - 1)) {
        std::fprintf(stderr, "ODBC_FAULT_TRACE data at execution for column %u\n", column);
        return real(stmt, column, direction, ctype, SQL_LONGVARCHAR, size, digits, data, buflen, &dataAtExec);
    }
    return real(stmt, column, direction, ctype, sqltype, size, digits, data, buflen, indicator);
}

extern "C" SQLRETURN SQL_API SQLDisconnect(SQLHDBC dbc) {
    using Disconnect = decltype(&SQLDisconnect);
    static Disconnect real = reinterpret_cast<Disconnect>(dlsym(RTLD_NEXT, "SQLDisconnect"));
    if (!real) {
        std::abort();
    }
    if (enabled("ODBC_FAULT_DISCONNECT")) {
        std::fprintf(stderr, "ODBC_FAULT_TRACE SQLDisconnect refused\n");
        return SQL_ERROR;
    }
    std::fprintf(stderr, "ODBC_FAULT_TRACE SQLDisconnect\n");
    return real(dbc);
}
