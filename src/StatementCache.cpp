/* -*- indent-tabs-mode: nil -*- */
/*
    StatementCache.cpp

    Qore ODBC module - Statement Cache implementation

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

#include "StatementCache.h"

#include <chrono>
#include <algorithm>

namespace odbc {

StatementCache::StatementCache(size_t maxSize) : maxCacheSize(maxSize) {
    stats.maxSize = maxSize;
}

StatementCache::~StatementCache() {
    clear();
}

void StatementCache::setMaxSize(size_t size) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    maxCacheSize = size;
    stats.maxSize = size;

    // Evict excess entries if needed
    while (cache.size() > maxCacheSize) {
        evictLRU();
    }
}

bool StatementCache::get(const std::string& sql, SQLHSTMT& stmt) {
    if (!enabled) {
        stats.misses++;
        return false;
    }

    std::lock_guard<std::mutex> lock(cacheMutex);

    auto it = cache.find(sql);
    if (it != cache.end()) {
        // Cache hit
        stmt = it->second.stmt;
        it->second.lastUsed = getCurrentTime();
        it->second.useCount++;
        touchLRU(sql);
        stats.hits++;
        return true;
    }

    // Cache miss
    stats.misses++;
    return false;
}

void StatementCache::put(const std::string& sql, SQLHSTMT stmt) {
    if (!enabled) {
        return;
    }

    std::lock_guard<std::mutex> lock(cacheMutex);

    // Check if already in cache
    auto it = cache.find(sql);
    if (it != cache.end()) {
        // Update existing entry
        // First free the old statement handle if different
        if (it->second.stmt != stmt) {
            SQLFreeHandle(SQL_HANDLE_STMT, it->second.stmt);
            it->second.stmt = stmt;
        }
        it->second.lastUsed = getCurrentTime();
        touchLRU(sql);
        return;
    }

    // Evict if at capacity
    while (cache.size() >= maxCacheSize) {
        evictLRU();
    }

    // Add new entry
    CacheEntry entry;
    entry.stmt = stmt;
    entry.sql = sql;
    entry.lastUsed = getCurrentTime();
    entry.useCount = 1;

    cache[sql] = entry;
    lruList.push_front(sql);
    stats.currentSize = cache.size();
}

bool StatementCache::remove(const std::string& sql) {
    std::lock_guard<std::mutex> lock(cacheMutex);

    auto it = cache.find(sql);
    if (it != cache.end()) {
        // Free the statement handle
        SQLFreeHandle(SQL_HANDLE_STMT, it->second.stmt);
        cache.erase(it);

        // Remove from LRU list
        lruList.remove(sql);
        stats.currentSize = cache.size();
        return true;
    }
    return false;
}

void StatementCache::clear() {
    std::lock_guard<std::mutex> lock(cacheMutex);

    // Free all statement handles
    for (auto& pair : cache) {
        SQLFreeHandle(SQL_HANDLE_STMT, pair.second.stmt);
    }

    cache.clear();
    lruList.clear();
    stats.currentSize = 0;
}

CacheStats StatementCache::getStats() const {
    std::lock_guard<std::mutex> lock(cacheMutex);
    stats.currentSize = cache.size();
    return stats;
}

void StatementCache::resetStats() {
    std::lock_guard<std::mutex> lock(cacheMutex);
    stats.hits = 0;
    stats.misses = 0;
    stats.evictions = 0;
    stats.currentSize = cache.size();
}

size_t StatementCache::size() const {
    std::lock_guard<std::mutex> lock(cacheMutex);
    return cache.size();
}

void StatementCache::evictLRU() {
    // Must be called with cacheMutex held
    if (lruList.empty()) {
        return;
    }

    // Get least recently used (back of list)
    const std::string& sql = lruList.back();

    auto it = cache.find(sql);
    if (it != cache.end()) {
        // Free the statement handle
        SQLFreeHandle(SQL_HANDLE_STMT, it->second.stmt);
        cache.erase(it);
        stats.evictions++;
    }

    lruList.pop_back();
    stats.currentSize = cache.size();
}

void StatementCache::touchLRU(const std::string& sql) {
    // Must be called with cacheMutex held
    // Move to front of LRU list (most recently used)
    lruList.remove(sql);
    lruList.push_front(sql);
}

int64_t StatementCache::getCurrentTime() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

} // namespace odbc
