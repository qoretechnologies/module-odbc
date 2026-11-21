/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ODBCConnectionString.h

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

#ifndef _QORE_MODULE_ODBC_ODBCCONNECTIONSTRING_H
#define _QORE_MODULE_ODBC_ODBCCONNECTIONSTRING_H

/** @file ODBCConnectionString.h writing, parsing and redacting ODBC connection strings

    A value is written in braces when it contains a character that would otherwise end or change it (\c ';', \c '{'
    or \c '}', or leading or trailing white space), with any \c '}' doubled, as the ODBC grammar requires; other
    values are written as they are, because some drivers remove the braces only from some keys (psqlODBC, for
    example, only from the password).

    The values of the connection that identify or authenticate it (the DSN, the database, the user name, the password
    and other secret keys) are never shown in messages: redact() replaces them in driver diagnostics.
*/

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace odbc_connstr {

//! a key of a connection string, upper-cased, and its value without braces
typedef std::pair<std::string, std::string> keyval_t;

//! the text that replaces a redacted value
static constexpr const char* MASK = "****";

static inline std::string upper(std::string s) {
    for (char& c : s) {
        c = toupper(static_cast<unsigned char>(c));
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

//! returns the value in braces, with any '}' doubled
static inline std::string brace(const std::string& v) {
    std::string rv = "{";
    for (char c : v) {
        rv += c;
        if (c == '}') {
            rv += '}';
        }
    }
    rv += '}';
    return rv;
}

//! true if the value must be written in braces to reach the driver unchanged
static inline bool needs_braces(const std::string& v) {
    if (v.find_first_of(";{}") != std::string::npos) {
        return true;
    }
    return !v.empty() && (isspace(static_cast<unsigned char>(v.front()))
        || isspace(static_cast<unsigned char>(v.back())));
}

//! returns the value as it is written in a connection string
static inline std::string quote_value(const std::string& v) {
    return needs_braces(v) ? brace(v) : v;
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

//! true if the value of the (upper-case) key identifies or authenticates the connection
/** The list is conservative: the DSN, the database, the user name, the password, and any key that names a
    password, token, secret, key or credential.
*/
static inline bool is_secret_key(const std::string& key) {
    static const char* exact[] = {"UID", "DB", "DBNAME", "DSN", "FILEDSN", "SAVEFILE", "PWD"};
    for (const char* k : exact) {
        if (key == k) {
            return true;
        }
    }
    static const char* parts[] = {"USER", "DATABASE", "CATALOG", "PASSWORD", "PWD", "PASSWD", "TOKEN", "SECRET",
        "KEY", "CREDENTIAL"};
    for (const char* p : parts) {
        if (key.find(p) != std::string::npos) {
            return true;
        }
    }
    return false;
}

//! adds the value to the list of values to redact, and also its trimmed form if it differs
static inline void add_secret(const std::string& v, std::vector<std::string>& secrets) {
    if (v.empty()) {
        return;
    }
    secrets.push_back(v);
    std::string t = trim(v);
    if (!t.empty() && t != v) {
        secrets.push_back(t);
    }
}

//! adds the secret values of a connection string to \a secrets
/** If the string cannot be parsed, every value and every element without a key is added, as it is not known how
    the driver splits it.
*/
static inline void add_secrets(const std::string& cs, std::vector<std::string>& secrets) {
    std::vector<keyval_t> kv;
    std::string err;
    if (parse_connection_string(cs, kv, err)) {
        for (const keyval_t& p : kv) {
            if (is_secret_key(p.first)) {
                add_secret(p.second, secrets);
            }
        }
        return;
    }
    // every element, split at each ';', '{', '}' and the first '=' of the element
    size_t i = 0;
    while (i <= cs.size()) {
        size_t e = cs.find(';', i);
        std::string elem = cs.substr(i, e == std::string::npos ? std::string::npos : e - i);
        size_t eq = elem.find('=');
        std::string value = eq == std::string::npos ? elem : elem.substr(eq + 1);
        size_t b = 0;
        while (b <= value.size()) {
            size_t n = value.find_first_of("{}", b);
            add_secret(value.substr(b, n == std::string::npos ? std::string::npos : n - b), secrets);
            if (n == std::string::npos) {
                break;
            }
            b = n + 1;
        }
        if (e == std::string::npos) {
            break;
        }
        i = e + 1;
    }
}

//! sorts the values to redact, longest first, so that a value that contains another one is replaced whole
static inline void finish_secrets(std::vector<std::string>& secrets) {
    std::sort(secrets.begin(), secrets.end(), [](const std::string& a, const std::string& b) {
        return a.size() != b.size() ? a.size() > b.size() : a < b;
    });
    secrets.erase(std::unique(secrets.begin(), secrets.end()), secrets.end());
}

//! replaces every occurrence of the values in \a text with MASK
/** The match is exact: servers show these values as they were given (for example \c user "name" or
    \c database "name"), and an exact match does not mask other text that differs only in case, such as the name
    of the driver for the user name \c postgres.

    @param text the text to redact
    @param secrets the values to replace, sorted by finish_secrets()
*/
static inline void redact(std::string& text, const std::vector<std::string>& secrets) {
    for (const std::string& s : secrets) {
        if (s.empty()) {
            continue;
        }
        size_t pos = 0;
        while ((pos = text.find(s, pos)) != std::string::npos) {
            text.replace(pos, s.size(), MASK);
            pos += strlen(MASK);
        }
    }
}

} // namespace odbc_connstr

#endif // _QORE_MODULE_ODBC_ODBCCONNECTIONSTRING_H
