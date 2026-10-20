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

#include <sql.h>
#include <sqlext.h>

#include "qore/Qore.h"

namespace odbc {

//! ODBC error helper
class ODBCErrorHelper {
public:
    //! Extract ODBC diagnostic records and output them to a stringstream.
    /** @param handleType type of the ODBC handle
        @param handle ODBC handle
        @param s string where the output will be written to
     */
    DLLLOCAL static void extractDiag(SQLSMALLINT handleType, SQLHANDLE& handle, std::string& s) {
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
            s += reinterpret_cast<char*>(text);
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

