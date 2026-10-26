/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ODBCCancelHelper.h

    Qore ODBC module

    Copyright (C) 2026 Qore Technologies s.r.o.

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

#ifndef _QORE_MODULE_ODBC_ODBCCANCELHELPER_H
#define _QORE_MODULE_ODBC_ODBCCANCELHELPER_H

#include <memory>
#include <mutex>

#include <sql.h>
#include <sqlext.h>

#include <qore/Qore.h>
#include <qore/QoreSandboxManager.h>

namespace odbc {

//! Lets a program interrupt cancel a blocking statement or connection call if supported by the ODBC driver
/** Create it on the stack around one blocking call (SQLExecDirect(), SQLExecute(), SQLMoreResults(),
    SQLFetch(), SQLDriverConnect()); check qore_check_cancel() before and after the call for thread cancellation.

    The sandbox manager calls copies of its registered callbacks outside its own lock, so a callback can still
    run after it has been unregistered.  The callback therefore holds only the shared state below: the
    destructor clears the handle under the state's lock, which waits for an SQLCancel() that is in progress, and
    no callback can reach the handle after the destructor has returned.
 */
class ODBCCancelHelper {
public:
    DLLLOCAL explicit ODBCCancelHelper(SQLHANDLE stmt, SQLSMALLINT type = SQL_HANDLE_STMT) {
        if (smh && stmt != SQL_NULL_HSTMT) {
            std::shared_ptr<State> s = std::make_shared<State>(stmt, type);
            smh->registerCancelCallback(this, [s]() -> bool {
                return s->cancel();
            });
            state = s;
        }
    }

    DLLLOCAL ~ODBCCancelHelper() {
        if (state) {
            smh->unregisterCancelCallback(this);
            state->clear();
        }
    }

    ODBCCancelHelper(const ODBCCancelHelper&) = delete;
    ODBCCancelHelper& operator=(const ODBCCancelHelper&) = delete;

private:
    //! The handle that a callback may cancel, shared with the registered callback
    class State {
    public:
        DLLLOCAL explicit State(SQLHANDLE stmt, SQLSMALLINT type) : stmt(stmt), type(type) {
        }

        //! Cancels the blocking call; returns true if SQLCancel() succeeded
        DLLLOCAL bool cancel() {
            std::lock_guard<std::mutex> lock(m);
            if (stmt == SQL_NULL_HSTMT) {
                return false;
            }
            return SQL_SUCCEEDED(type == SQL_HANDLE_STMT ? SQLCancel(stmt) : SQLCancelHandle(type, stmt));
        }

        //! Makes later callbacks do nothing; waits for a running SQLCancel()
        DLLLOCAL void clear() {
            std::lock_guard<std::mutex> lock(m);
            stmt = SQL_NULL_HSTMT;
        }

    private:
        std::mutex m;
        SQLHANDLE stmt;
        const SQLSMALLINT type;
    };

    //! the governing sandbox, whose interrupt cancels the running query; not a policy check, so a
    //! system-policy capability (a policy barrier) does not stop the sandbox from interrupting it
    QoreSandboxManagerHelper smh{QoreSandboxManagerHelper::All};
    std::shared_ptr<State> state;
};

} // namespace odbc

#endif // _QORE_MODULE_ODBC_ODBCCANCELHELPER_H
