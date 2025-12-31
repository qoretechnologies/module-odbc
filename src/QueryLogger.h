/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QueryLogger.h

    Qore ODBC module - Query logging and profiling

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

#ifndef _QORE_MODULE_ODBC_QUERYLOGGER_H
#define _QORE_MODULE_ODBC_QUERYLOGGER_H

#include "qore/Qore.h"

#include <string>
#include <chrono>
#include <mutex>
#include <cstdint>
#include <functional>

namespace odbc {

//! Log levels for query logging
enum class LogLevel {
    OFF = 0,    //!< No logging
    ERROR = 1,  //!< Log only errors
    WARN = 2,   //!< Log warnings and errors
    INFO = 3,   //!< Log info, warnings, and errors
    TRACE = 4   //!< Log everything including trace/debug info
};

//! Statistics about query execution
struct QueryStats {
    uint64_t totalQueries = 0;      //!< Total number of queries executed
    uint64_t totalErrors = 0;       //!< Total number of errors
    uint64_t slowQueries = 0;       //!< Number of slow queries
    double totalExecutionTimeMs = 0; //!< Total execution time in milliseconds
    double avgExecutionTimeMs = 0;   //!< Average execution time
    double maxExecutionTimeMs = 0;   //!< Maximum execution time
    double minExecutionTimeMs = 0;   //!< Minimum execution time (0 if no queries)
};

//! Query Logger for ODBC operations
/** Provides logging and profiling capabilities for ODBC queries.
    Can be used to identify slow queries and debug database operations.

    Usage:
    @code
    QueryLogger logger;
    logger.setEnabled(true);
    logger.setLogLevel(LogLevel::INFO);
    logger.setSlowQueryThresholdMs(1000); // 1 second

    // In query execution:
    auto timer = logger.startQuery("SELECT * FROM users");
    // ... execute query ...
    logger.endQuery(timer, true); // true = success
    @endcode
*/
class QueryLogger {
public:
    //! Callback type for log output
    using LogCallback = std::function<void(LogLevel level, const std::string& message)>;

    //! Timer class for measuring query execution time
    class QueryTimer {
    public:
        QueryTimer() : startTime(std::chrono::high_resolution_clock::now()) {}

        //! Get elapsed time in milliseconds
        double elapsedMs() const {
            auto now = std::chrono::high_resolution_clock::now();
            return std::chrono::duration<double, std::milli>(now - startTime).count();
        }

        //! Get the SQL text being timed
        const std::string& getSql() const { return sql; }

        //! Set the SQL text
        void setSql(const std::string& s) { sql = s; }

    private:
        std::chrono::high_resolution_clock::time_point startTime;
        std::string sql;
    };

    //! Constructor
    DLLLOCAL QueryLogger();

    //! Destructor
    DLLLOCAL ~QueryLogger() = default;

    //! Check if logging is enabled
    DLLLOCAL bool isEnabled() const { return enabled; }

    //! Enable or disable logging
    DLLLOCAL void setEnabled(bool enable) { enabled = enable; }

    //! Get current log level
    DLLLOCAL LogLevel getLogLevel() const { return logLevel; }

    //! Set log level
    DLLLOCAL void setLogLevel(LogLevel level) { logLevel = level; }

    //! Get slow query threshold in milliseconds
    DLLLOCAL double getSlowQueryThresholdMs() const { return slowQueryThresholdMs; }

    //! Set slow query threshold in milliseconds
    /** Queries taking longer than this threshold will be logged as warnings.
        @param thresholdMs threshold in milliseconds (default: 1000)
     */
    DLLLOCAL void setSlowQueryThresholdMs(double thresholdMs) {
        slowQueryThresholdMs = thresholdMs;
    }

    //! Check if parameter logging is enabled
    DLLLOCAL bool isLogParameters() const { return logParameters; }

    //! Enable or disable parameter logging
    /** When enabled, query parameters will be included in log output.
        Be careful with sensitive data!
        @param enable true to log parameters
     */
    DLLLOCAL void setLogParameters(bool enable) { logParameters = enable; }

    //! Set a callback for log output
    /** @param callback function to call for each log message
     */
    DLLLOCAL void setLogCallback(LogCallback callback) {
        std::lock_guard<std::mutex> lock(statsMutex);
        logCallback = callback;
    }

    //! Start timing a query
    /** @param sql the SQL text being executed
        @return a timer object to pass to endQuery()
     */
    DLLLOCAL QueryTimer startQuery(const std::string& sql);

    //! End timing a query and log results
    /** @param timer the timer from startQuery()
        @param success whether the query succeeded
        @param errorMsg optional error message if query failed
     */
    DLLLOCAL void endQuery(const QueryTimer& timer, bool success,
                           const std::string& errorMsg = "");

    //! Log a message at the specified level
    /** @param level the log level
        @param message the message to log
     */
    DLLLOCAL void log(LogLevel level, const std::string& message);

    //! Get query statistics
    DLLLOCAL QueryStats getStats() const;

    //! Reset statistics
    DLLLOCAL void resetStats();

    //! Format a duration in milliseconds for display
    DLLLOCAL static std::string formatDuration(double ms);

private:
    //! Whether logging is enabled
    bool enabled = false;

    //! Current log level
    LogLevel logLevel = LogLevel::INFO;

    //! Threshold for slow query warnings (in milliseconds)
    double slowQueryThresholdMs = 1000.0;

    //! Whether to log query parameters
    bool logParameters = false;

    //! Callback for log output
    LogCallback logCallback;

    //! Mutex for thread-safe stats updates
    mutable std::mutex statsMutex;

    //! Query statistics
    QueryStats stats;

    //! Check if a message should be logged at the given level
    DLLLOCAL bool shouldLog(LogLevel level) const {
        return enabled && static_cast<int>(level) <= static_cast<int>(logLevel);
    }

    //! Get string representation of log level
    DLLLOCAL static const char* logLevelToString(LogLevel level);
};

} // namespace odbc

#endif // _QORE_MODULE_ODBC_QUERYLOGGER_H
