/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ODBCConnectTargets.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_ODBC_CONNECT_TARGETS_H
#define _QORE_ODBC_CONNECT_TARGETS_H

/** @file ODBCConnectTargets.h the network destinations of an ODBC connection, for the sandbox of the code that chose
    it (QDBI_METHOD_GET_CONNECT_TARGETS)

    Where an ODBC connection goes depends on its driver, so the destinations are determined only for the drivers
    known here, from the keys of the connection string merged with those of its DSN, which are read through the
    driver manager (SQLGetPrivateProfileString(), never parsed here), so that the resolution cannot differ from the
    driver manager's:
    - PostgreSQL (psqlODBC): \c Servername or \c Server, \c Port (5432); the connection is pinned to the checked
      address with <tt>pqopt={hostaddr=...}</tt>, which keeps the host name for TLS;
    - MySQL and MariaDB (Connector/ODBC): \c Server, \c Port (3306); pinned by replacing the server, so a mode that
      verifies the server's host name (\c SSLMODE=VERIFY_IDENTITY, \c SSL_VERIFY_SERVER_CERT) is refused.

    The Microsoft ODBC Driver for SQL Server is refused: the server can route the connection to another server
    during the login (TDS routing), which the driver follows without a check, so the destination is not known
    beforehand.

    Everything else is refused with \c NETWORK-ACCESS-DENIED and the reason: another driver (named in the message),
    the Microsoft ODBC Driver for SQL Server and the FreeTDS driver (TDS routing), the SQLite driver, a DSN that is
    not found, a connection string with both \c DSN and \c DRIVER, a file DSN, a local socket or an empty host, a
    host list, and libpq options (\c pqopt) set by the connection itself.
*/

#include <odbcinst.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace qore_odbc_targets {

//! a key of a connection string, upper-cased, and its value without braces
typedef std::pair<std::string, std::string> keyval_t;

static inline std::string upper(std::string s) {
    for (char& c : s) {
        c = toupper(static_cast<unsigned char>(c));
    }
    return s;
}

static inline std::string lower(std::string s) {
    for (char& c : s) {
        c = tolower(static_cast<unsigned char>(c));
    }
    return s;
}

static inline std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) {
        return std::string();
    }
    return s.substr(b, s.find_last_not_of(" \t") - b + 1);
}

//! parses a connection string: <tt>key=value;...</tt>, a value in braces can contain ';' (and "}}" for '}')
static inline bool parse_connection_string(const std::string& s, std::vector<keyval_t>& kv, std::string& err) {
    size_t i = 0;
    while (i < s.size()) {
        size_t eq = s.find('=', i);
        size_t semi = s.find(';', i);
        if (eq == std::string::npos || (semi != std::string::npos && semi < eq)) {
            // an empty element, or one without a value
            if (!trim(s.substr(i, semi == std::string::npos ? std::string::npos : semi - i)).empty()) {
                err = "the connection string has an element without '='";
                return false;
            }
            if (semi == std::string::npos) {
                break;
            }
            i = semi + 1;
            continue;
        }
        std::string key = upper(trim(s.substr(i, eq - i)));
        i = eq + 1;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
            ++i;
        }
        std::string value;
        if (i < s.size() && s[i] == '{') {
            ++i;
            bool closed = false;
            while (i < s.size()) {
                if (s[i] == '}') {
                    if (i + 1 < s.size() && s[i + 1] == '}') {
                        value += '}';
                        i += 2;
                        continue;
                    }
                    ++i;
                    closed = true;
                    break;
                }
                value += s[i++];
            }
            if (!closed) {
                err = "a value in braces is not closed in the connection string";
                return false;
            }
            while (i < s.size() && s[i] != ';') {
                if (!isspace(static_cast<unsigned char>(s[i]))) {
                    err = "unexpected text after a value in braces in the connection string";
                    return false;
                }
                ++i;
            }
        } else {
            size_t e = s.find(';', i);
            value = trim(s.substr(i, e == std::string::npos ? std::string::npos : e - i));
            i = e == std::string::npos ? s.size() : e;
        }
        if (i < s.size() && s[i] == ';') {
            ++i;
        }
        kv.emplace_back(key, value);
    }
    return true;
}

//! returns the value of the first occurrence of the key, as the driver uses it
static inline const std::string* find_key(const std::vector<keyval_t>& kv, const char* key) {
    for (const keyval_t& p : kv) {
        if (p.first == key) {
            return &p.second;
        }
    }
    return nullptr;
}

//! reads a value from odbc.ini or odbcinst.ini through the driver manager
static inline std::string profile_value(const std::string& section, const char* key, const char* file) {
    char buf[1024];
    // the 8-bit function (UNICODE maps the name to the UTF-16 one): the names and values here are ASCII
#pragma push_macro("SQLGetPrivateProfileString")
#undef SQLGetPrivateProfileString
    int len = SQLGetPrivateProfileString(section.c_str(), key, "", buf, sizeof buf, file);
#pragma pop_macro("SQLGetPrivateProfileString")
    return len > 0 ? std::string(buf, std::min<size_t>(len, sizeof buf - 1)) : std::string();
}

//! the keys of the connection, from the connection string and its DSN (the connection string first)
class ConnectionKeys {
public:
    std::vector<keyval_t> kv;
    std::string dsn;

    //! returns the value of the key: from the connection string, else from the DSN, else empty
    std::string get(const char* key) const {
        if (const std::string* v = find_key(kv, key)) {
            return *v;
        }
        if (!dsn.empty()) {
            return profile_value(dsn, key, "odbc.ini");
        }
        return std::string();
    }

    //! true if the key is set in the connection string or the DSN
    bool has(const char* key) const {
        if (find_key(kv, key)) {
            return true;
        }
        return !dsn.empty() && !profile_value(dsn, key, "odbc.ini").empty();
    }
};

//! the drivers whose destinations are known
enum DriverKind { DK_POSTGRESQL, DK_MYSQL };

//! a destination: a host and a port
struct Target {
    std::string host;
    int port = 0;
};

//! identifies the driver of the connection; returns false with err set if it is not known
static inline bool get_driver(const ConnectionKeys& ck, DriverKind& kind, std::string& err) {
    std::string driver = ck.get("DRIVER");
    if (driver.empty()) {
        err = "the ODBC driver of the connection cannot be determined";
        return false;
    }
    // a driver name from odbcinst.ini, or the path of its library
    std::string path = driver;
    if (driver.find('/') == std::string::npos) {
        path = profile_value(driver, "Driver", "odbcinst.ini");
        if (path.empty()) {
            err = "the ODBC driver '" + driver + "' is not defined in odbcinst.ini";
            return false;
        }
    }
    size_t s = path.rfind('/');
    std::string base = lower(s == std::string::npos ? path : path.substr(s + 1));
    if (base.find("psqlodbc") != std::string::npos) {
        kind = DK_POSTGRESQL;
    } else if (base.find("myodbc") != std::string::npos || base.find("maodbc") != std::string::npos) {
        kind = DK_MYSQL;
    } else if (base.find("msodbcsql") != std::string::npos) {
        err = "connections with the Microsoft ODBC Driver for SQL Server cannot be checked against the sandbox: the "
            "server can route the connection to another server during the login (TDS routing)";
        return false;
    } else if (base.find("tdsodbc") != std::string::npos) {
        err = "connections with the FreeTDS ODBC driver cannot be checked against the sandbox: the server can route "
            "the connection to another server during the login (TDS routing)";
        return false;
    } else if (base.find("sqlite") != std::string::npos) {
        err = "connections with the SQLite ODBC driver cannot be checked against the sandbox";
        return false;
    } else {
        err = "the destinations of connections with the ODBC driver '" + driver + "' (" + path + ") cannot be "
            "determined";
        return false;
    }
    return true;
}

static inline bool parse_port(const std::string& v, int dflt, int& port, std::string& err) {
    if (v.empty()) {
        port = dflt;
        return true;
    }
    char* end;
    long n = strtol(v.c_str(), &end, 10);
    if (*end || n <= 0 || n > 65535) {
        err = "invalid port '" + v + "'";
        return false;
    }
    port = (int)n;
    return true;
}

static inline bool is_true(const std::string& v) {
    std::string u = upper(trim(v));
    return u == "1" || u == "YES" || u == "TRUE" || u == "ON";
}

//! determines the keys and driver of the connection string and its destination; returns false with err set if
//! they cannot be determined
static inline bool get_target(const std::string& conn_str, ConnectionKeys& ck, DriverKind& kind, Target& t,
        std::string& err) {
    if (!parse_connection_string(conn_str, ck.kv, err)) {
        return false;
    }
    if (find_key(ck.kv, "FILEDSN") || find_key(ck.kv, "SAVEFILE")) {
        err = "a file DSN cannot be checked against the sandbox";
        return false;
    }
    const std::string* dsn = find_key(ck.kv, "DSN");
    if (dsn && find_key(ck.kv, "DRIVER")) {
        err = "the connection string has both DSN and DRIVER";
        return false;
    }
    if (dsn) {
        ck.dsn = *dsn;
        // the DSN must exist: it has a Driver entry
        if (profile_value(ck.dsn, "Driver", "odbc.ini").empty()) {
            err = "the ODBC DSN '" + ck.dsn + "' is not defined";
            return false;
        }
    }
    if (!get_driver(ck, kind, err)) {
        return false;
    }
    switch (kind) {
        case DK_POSTGRESQL: {
            t.host = ck.has("SERVERNAME") ? ck.get("SERVERNAME") : ck.get("SERVER");
            if (ck.has("PQOPT")) {
                err = "libpq options (pqopt) cannot be checked against the sandbox";
                return false;
            }
            if (!parse_port(ck.get("PORT"), 5432, t.port, err)) {
                return false;
            }
            break;
        }
        case DK_MYSQL: {
            t.host = ck.get("SERVER");
            if (ck.has("SOCKET") || t.host.empty() || lower(t.host) == "localhost") {
                err = "a connection to a local socket of the MySQL server cannot be checked against the sandbox";
                return false;
            }
            if (is_true(ck.get("MULTI_HOST"))) {
                err = "a MySQL connection to a list of servers cannot be checked against the sandbox";
                return false;
            }
            std::string sslmode = upper(trim(ck.get("SSLMODE")));
            if (sslmode == "VERIFY_IDENTITY" || is_true(ck.get("SSL_VERIFY_SERVER_CERT"))
                    || is_true(ck.get("SSLVERIFY"))) {
                err = "a MySQL connection that verifies the host name of the server cannot be checked against the "
                    "sandbox yet: the driver would connect to the checked numeric address";
                return false;
            }
            if (!parse_port(ck.get("PORT"), 3306, t.port, err)) {
                return false;
            }
            break;
        }
    }
    if (t.host.empty()) {
        err = "a connection to a local socket of the server cannot be checked against the sandbox";
        return false;
    }
    if (t.host.find(',') != std::string::npos) {
        err = "a connection to a list of servers cannot be checked against the sandbox";
        return false;
    }
    return true;
}

//! returns the keys that pin the connection to \a addr, to put before the connection string (the first occurrence of
//! a key is the one that the driver uses)
static inline std::string pin_keys(const ConnectionKeys& ck, DriverKind kind, const Target& t,
        const std::string& addr) {
    auto brace = [](const std::string& v) {
        std::string rv = "{";
        for (char c : v) {
            rv += c;
            if (c == '}') {
                rv += '}';
            }
        }
        return rv + "}";
    };
    switch (kind) {
        case DK_POSTGRESQL:
            // libpq connects to hostaddr and keeps the host name for TLS
            return "Servername=" + brace(t.host) + ";Port=" + std::to_string(t.port) + ";pqopt="
                + brace("hostaddr=" + addr) + ";";
        case DK_MYSQL:
            return "Server=" + brace(addr) + ";Port=" + std::to_string(t.port) + ";";
    }
    return std::string();
}

#ifdef QDBI_METHOD_GET_CONNECT_TARGETS
//! returns the destinations of a connection of the datasource (QDBI_METHOD_GET_CONNECT_TARGETS)
static inline QoreListNode* get_connect_targets(Datasource* ds, ExceptionSink* xsink) {
    QoreString conn_str(QCS_UTF8);
    if (odbc::ODBCConnection::prepareConnectionString(ds, conn_str, xsink)) {
        return nullptr;
    }
    ConnectionKeys ck;
    DriverKind kind;
    Target t;
    std::string err;
    if (!get_target(conn_str.c_str(), ck, kind, t, err)) {
        xsink->raiseException("NETWORK-ACCESS-DENIED", "the destination of the ODBC connection cannot be checked "
            "against the sandbox: %s", err.c_str());
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
    h->setKeyValue("host", new QoreStringNode(t.host), xsink);
    h->setKeyValue("port", t.port, xsink);
    rv->push(h.release(), xsink);
    return rv.release();
}

//! returns in \a out the connection string of a connection that was checked against the sandbox: pinned to the
//! first checked address of its destination; returns 0 for success, -1 if an exception was raised
static inline int checked_connection_string(const QoreString& conn_str, const QoreListNode* checked,
        QoreString& out, ExceptionSink* xsink) {
    ConnectionKeys ck;
    DriverKind kind;
    Target t;
    std::string err;
    if (!get_target(conn_str.c_str(), ck, kind, t, err)) {
        xsink->raiseException("NETWORK-ACCESS-DENIED", "the destination of the ODBC connection cannot be checked "
            "against the sandbox: %s", err.c_str());
        return -1;
    }
    // the destination is determined again (odbc.ini could have changed) and must be the checked one
    QoreValue v = checked->size() == 1 ? checked->retrieveEntry(0) : QoreValue();
    const QoreHashNode* h = v.getType() == NT_HASH ? v.get<const QoreHashNode>() : nullptr;
    QoreValue host = h ? h->getKeyValue("host") : QoreValue();
    QoreValue addrs = h ? h->getKeyValue("addresses") : QoreValue();
    if (host.getType() != NT_STRING || host.get<const QoreStringNode>()->compare(t.host.c_str())
            || h->getKeyValue("port").getAsBigInt() != t.port || addrs.getType() != NT_LIST
            || addrs.get<const QoreListNode>()->retrieveEntry(0).getType() != NT_STRING) {
        xsink->raiseException("NETWORK-ACCESS-DENIED", "the destination of the ODBC connection changed after it "
            "was checked against the sandbox");
        return -1;
    }
    std::string addr = addrs.get<const QoreListNode>()->retrieveEntry(0).get<const QoreStringNode>()->c_str();
    out.set(pin_keys(ck, kind, t, addr).c_str(), QCS_UTF8);
    out.concat(&conn_str, xsink);
    return *xsink ? -1 : 0;
}
#endif

}  // namespace qore_odbc_targets

#endif
