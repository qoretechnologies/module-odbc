/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ODBCErrorHelper.h

  Qore ODBC module

  Copyright (C) 2016 - 2026 Qore Technologies s.r.o.

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
*/

#ifndef _QORE_MODULE_ODBC_ODBCERRORHELPER_H
#define _QORE_MODULE_ODBC_ODBCERRORHELPER_H

#include <cstdarg>
#include <string>
#include <vector>

#include <sql.h>
#include <sqlext.h>

#include "qore/Qore.h"

#include "ODBCConnectionString.h"

namespace odbc {

//! ODBC error helper
class ODBCErrorHelper {
public:
    //! Extract ODBC diagnostic records and output them to a stringstream.
    /** @param handleType type of the ODBC handle
        @param handle ODBC handle
        @param s string where the output will be written to
        @param secrets if given, the values masked in the message of each record (not in its SQLSTATE or native code)
     */
    DLLLOCAL static void extractDiag(SQLSMALLINT handleType, SQLHANDLE& handle, std::string& s,
            const std::vector<std::string>* secrets = nullptr) {
        SQLINTEGER i = 0;
        SQLINTEGER native;
        SQLCHAR state[7];
        SQLCHAR text[512];
        SQLSMALLINT len;

        while (true) {
            SQLRETURN ret = SQLGetDiagRecA(handleType, handle, ++i, state, &native, text,
                (SQLSMALLINT)sizeof(text), &len);
            if (!SQL_SUCCEEDED(ret)) {
                break;
            }
            s += "; [";
            s += reinterpret_cast<char*>(state);
            s += "] (native ";
            s += std::to_string(native);
            s += "): ";
            // a longer message is read whole, so that no value in it is cut (and could not be masked whole)
            std::string msg;
            if (len >= (SQLSMALLINT)sizeof(text) && len < 32767) {
                std::vector<SQLCHAR> buf(static_cast<size_t>(len) + 1);
                SQLSMALLINT len2;
                if (SQL_SUCCEEDED(SQLGetDiagRecA(handleType, handle, i, state, &native, buf.data(),
                        static_cast<SQLSMALLINT>(buf.size()), &len2))) {
                    msg = reinterpret_cast<char*>(buf.data());
                } else {
                    msg = reinterpret_cast<char*>(text);
                }
            } else {
                msg = reinterpret_cast<char*>(text);
            }
            // only the message is masked: the SQLSTATE and native code are kept as they are
            if (secrets) {
                odbc_connstr::redact(msg, *secrets);
            }
            s += msg;
        }
    }

    //! Raises an exception: the formatted description followed by the ODBC diagnostic records of the handle
    /** Only \a fmt is a format string: the diagnostics (which can contain SQL text and object names, and so any
        \c % character) are added as text, as must be any SQL or driver text, which is passed as an argument.

        @param xsink exception sink
        @param err exception code
        @param handleType type of the ODBC handle
        @param handle ODBC handle
        @param fmt format string for the description
     */
    DLLLOCAL static void raiseException(ExceptionSink* xsink, const char* err, SQLSMALLINT handleType,
            SQLHANDLE handle, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 5, 6)))
#endif
    {
        QoreStringNodeHolder desc(new QoreStringNode);
        while (true) {
            va_list args;
            va_start(args, fmt);
            int rc = desc->vsprintf(fmt, args);
            va_end(args);
            if (!rc) {
                break;
            }
        }
        std::string diag;
        extractDiag(handleType, handle, diag);
        desc->concat(diag.c_str());
        xsink->raiseException(err, desc.release());
    }

    //! Raises an exception like raiseException(), with the given connection values masked in the diagnostics
    /** Driver and server diagnostics of a connection can name its user, database or DSN (for example "password
        authentication failed for user ..."), so every occurrence of the values is replaced with
        odbc_connstr::MASK; the SQLSTATE and the rest of each record are kept. The description given with \a fmt
        is not changed: it must not contain any connection value.

        @param xsink exception sink
        @param err exception code
        @param handleType type of the ODBC handle
        @param handle ODBC handle
        @param secrets the values to mask, sorted by odbc_connstr::finish_secrets()
        @param fmt format string for the description
     */
    DLLLOCAL static void raiseRedactedException(ExceptionSink* xsink, const char* err, SQLSMALLINT handleType,
            SQLHANDLE handle, const std::vector<std::string>& secrets, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 6, 7)))
#endif
    {
        QoreStringNodeHolder desc(new QoreStringNode);
        while (true) {
            va_list args;
            va_start(args, fmt);
            int rc = desc->vsprintf(fmt, args);
            va_end(args);
            if (!rc) {
                break;
            }
        }
        std::string diag;
        extractDiag(handleType, handle, diag, &secrets);
        desc->concat(diag.c_str());
        xsink->raiseException(err, desc.release());
    }

    //! Extract ODBC state for the first record.
    /** @param handleType type of the ODBC handle
        @param handle ODBC handle
        @param buf buffer where the ODBC state will be written to
     */
    DLLLOCAL static void extractState(SQLSMALLINT handleType, SQLHANDLE& handle, char* buf) {
        SQLINTEGER native;
        SQLSMALLINT textLen;
        SQLGetDiagRecA(handleType, handle, 1, reinterpret_cast<SQLCHAR*>(buf), &native, 0, 0, &textLen);
    }

private:
    DLLLOCAL ODBCErrorHelper();
    DLLLOCAL ~ODBCErrorHelper();
};

} // namespace odbc

#endif // _QORE_MODULE_ODBC_ODBCERRORHELPER_H

