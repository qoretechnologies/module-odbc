/* -*- indent-tabs-mode: nil -*- */
/*
    QueryLogger.cpp

    Qore ODBC module - Query logging and profiling implementation

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

#include "QueryLogger.h"

#include <sstream>
#include <iomanip>
#include <cstdio>

namespace odbc {

QueryLogger::QueryLogger() {
    stats.minExecutionTimeMs = 0;
}

QueryLogger::QueryTimer QueryLogger::startQuery(const std::string& sql) {
    QueryTimer timer;
    timer.setSql(sql);

    if (shouldLog(LogLevel::TRACE)) {
        log(LogLevel::TRACE, "Starting query: " + sql);
    }

    return timer;
}

void QueryLogger::endQuery(const QueryTimer& timer, bool success,
                           const std::string& errorMsg) {
    double elapsed = timer.elapsedMs();

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(statsMutex);
        stats.totalQueries++;
        stats.totalExecutionTimeMs += elapsed;

        if (stats.totalQueries == 1) {
            stats.minExecutionTimeMs = elapsed;
        } else if (elapsed < stats.minExecutionTimeMs) {
            stats.minExecutionTimeMs = elapsed;
        }

        if (elapsed > stats.maxExecutionTimeMs) {
            stats.maxExecutionTimeMs = elapsed;
        }

        stats.avgExecutionTimeMs = stats.totalExecutionTimeMs / stats.totalQueries;

        if (!success) {
            stats.totalErrors++;
        }

        if (elapsed > slowQueryThresholdMs) {
            stats.slowQueries++;
        }
    }

    // Log based on result
    if (!success) {
        if (shouldLog(LogLevel::ERROR)) {
            std::ostringstream oss;
            oss << "Query FAILED after " << formatDuration(elapsed)
                << ": " << timer.getSql();
            if (!errorMsg.empty()) {
                oss << " - Error: " << errorMsg;
            }
            log(LogLevel::ERROR, oss.str());
        }
    } else if (elapsed > slowQueryThresholdMs) {
        if (shouldLog(LogLevel::WARN)) {
            std::ostringstream oss;
            oss << "SLOW QUERY (" << formatDuration(elapsed)
                << ", threshold: " << formatDuration(slowQueryThresholdMs)
                << "): " << timer.getSql();
            log(LogLevel::WARN, oss.str());
        }
    } else {
        if (shouldLog(LogLevel::TRACE)) {
            std::ostringstream oss;
            oss << "Query completed in " << formatDuration(elapsed)
                << ": " << timer.getSql();
            log(LogLevel::TRACE, oss.str());
        }
    }
}

void QueryLogger::log(LogLevel level, const std::string& message) {
    if (!shouldLog(level)) {
        return;
    }

    std::lock_guard<std::mutex> lock(statsMutex);

    if (logCallback) {
        logCallback(level, message);
    } else {
        // Default: print to stderr
        fprintf(stderr, "[ODBC %s] %s\n", logLevelToString(level), message.c_str());
    }
}

QueryStats QueryLogger::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex);
    return stats;
}

void QueryLogger::resetStats() {
    std::lock_guard<std::mutex> lock(statsMutex);
    stats = QueryStats();
}

std::string QueryLogger::formatDuration(double ms) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    if (ms < 1.0) {
        oss << (ms * 1000.0) << " us";
    } else if (ms < 1000.0) {
        oss << ms << " ms";
    } else {
        oss << (ms / 1000.0) << " s";
    }

    return oss.str();
}

const char* QueryLogger::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::OFF:   return "OFF";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::TRACE: return "TRACE";
        default:              return "UNKNOWN";
    }
}

} // namespace odbc
