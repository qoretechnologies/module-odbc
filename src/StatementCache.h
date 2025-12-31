/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    StatementCache.h

    Qore ODBC module - Statement Cache for prepared statements

    Copyright (C) 2016 - 2024 Qore Technologies s.r.o.

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

#ifndef _QORE_MODULE_ODBC_STATEMENTCACHE_H
#define _QORE_MODULE_ODBC_STATEMENTCACHE_H

#include <sql.h>
#include <sqlext.h>

#include "qore/Qore.h"

#include <string>
#include <unordered_map>
#include <list>
#include <mutex>
#include <cstdint>

namespace odbc {

//! Statistics for the statement cache
struct CacheStats {
    uint64_t hits = 0;      //!< Number of cache hits
    uint64_t misses = 0;    //!< Number of cache misses
    uint64_t evictions = 0; //!< Number of evictions
    size_t currentSize = 0; //!< Current number of cached statements
    size_t maxSize = 0;     //!< Maximum cache size
};

//! Entry in the statement cache
struct CacheEntry {
    SQLHSTMT stmt;           //!< The prepared statement handle
    std::string sql;         //!< The SQL text
    int64_t lastUsed;        //!< Timestamp of last use (for LRU)
    uint64_t useCount;       //!< Number of times this statement was used
};

//! LRU Cache for prepared ODBC statements
/** This class implements a thread-safe LRU (Least Recently Used) cache
    for prepared ODBC statements. Caching prepared statements can significantly
    improve performance for frequently executed queries.

    Usage:
    @code
    StatementCache cache(100); // Cache up to 100 statements
    cache.setEnabled(true);

    SQLHSTMT stmt;
    if (cache.get("SELECT * FROM users WHERE id = ?", stmt)) {
        // Use cached statement
    } else {
        // Prepare new statement and add to cache
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
        SQLPrepare(stmt, sql, SQL_NTS);
        cache.put("SELECT * FROM users WHERE id = ?", stmt);
    }
    @endcode
*/
class StatementCache {
public:
    //! Constructor
    /** @param maxSize Maximum number of statements to cache (default: 100)
     */
    DLLLOCAL explicit StatementCache(size_t maxSize = 100);

    //! Destructor - frees all cached statement handles
    DLLLOCAL ~StatementCache();

    //! Disabled copy constructor
    DLLLOCAL StatementCache(const StatementCache&) = delete;

    //! Disabled assignment operator
    DLLLOCAL StatementCache& operator=(const StatementCache&) = delete;

    //! Check if caching is enabled
    /** @return true if caching is enabled
     */
    DLLLOCAL bool isEnabled() const { return enabled; }

    //! Enable or disable the cache
    /** @param enable true to enable, false to disable
     */
    DLLLOCAL void setEnabled(bool enable) { enabled = enable; }

    //! Get the maximum cache size
    /** @return maximum number of statements that can be cached
     */
    DLLLOCAL size_t getMaxSize() const { return maxCacheSize; }

    //! Set the maximum cache size
    /** If the new size is smaller than the current number of cached statements,
        excess statements will be evicted using LRU policy.
        @param size new maximum size
     */
    DLLLOCAL void setMaxSize(size_t size);

    //! Try to get a cached prepared statement
    /** @param sql the SQL text to look up
        @param stmt output parameter for the statement handle if found
        @return true if found in cache, false otherwise
     */
    DLLLOCAL bool get(const std::string& sql, SQLHSTMT& stmt);

    //! Add a prepared statement to the cache
    /** If the cache is full, the least recently used statement will be evicted.
        @param sql the SQL text
        @param stmt the prepared statement handle
     */
    DLLLOCAL void put(const std::string& sql, SQLHSTMT stmt);

    //! Remove a specific statement from the cache
    /** @param sql the SQL text to remove
        @return true if the statement was found and removed
     */
    DLLLOCAL bool remove(const std::string& sql);

    //! Clear all cached statements
    /** Frees all statement handles and clears the cache.
     */
    DLLLOCAL void clear();

    //! Get cache statistics
    /** @return statistics about cache usage
     */
    DLLLOCAL CacheStats getStats() const;

    //! Reset statistics counters
    DLLLOCAL void resetStats();

    //! Get current number of cached statements
    /** @return number of statements in cache
     */
    DLLLOCAL size_t size() const;

private:
    //! Whether caching is enabled
    bool enabled = false;

    //! Maximum number of statements to cache
    size_t maxCacheSize;

    //! Map from SQL text to cache entry
    std::unordered_map<std::string, CacheEntry> cache;

    //! LRU list - front is most recently used, back is least recently used
    std::list<std::string> lruList;

    //! Mutex for thread safety
    mutable std::mutex cacheMutex;

    //! Cache statistics
    mutable CacheStats stats;

    //! Evict the least recently used entry
    /** Must be called with cacheMutex held.
     */
    DLLLOCAL void evictLRU();

    //! Move an entry to the front of the LRU list
    /** Must be called with cacheMutex held.
        @param sql the SQL text to move to front
     */
    DLLLOCAL void touchLRU(const std::string& sql);

    //! Get current timestamp in milliseconds
    DLLLOCAL static int64_t getCurrentTime();
};

} // namespace odbc

#endif // _QORE_MODULE_ODBC_STATEMENTCACHE_H
