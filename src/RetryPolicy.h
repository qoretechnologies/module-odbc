/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    RetryPolicy.h

    Qore ODBC module - Retry policy for transient failures

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

#ifndef _QORE_MODULE_ODBC_RETRYPOLICY_H
#define _QORE_MODULE_ODBC_RETRYPOLICY_H

#include "qore/Qore.h"

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <thread>
#include <random>
#include <cstdint>

namespace odbc {

//! Statistics about retry operations
struct RetryStats {
    uint64_t totalAttempts = 0;     //!< Total number of attempts
    uint64_t totalRetries = 0;      //!< Total number of retries (attempts - successes on first try)
    uint64_t totalSuccesses = 0;    //!< Total successful operations
    uint64_t totalFailures = 0;     //!< Total failed operations (after all retries)
    double totalWaitTimeMs = 0;     //!< Total time spent waiting between retries
};

//! Retry policy configuration
struct RetryConfig {
    bool enabled = false;               //!< Whether retry is enabled
    int maxAttempts = 3;                //!< Maximum number of attempts (including first)
    double initialDelayMs = 100.0;      //!< Initial delay between retries in milliseconds
    double maxDelayMs = 30000.0;        //!< Maximum delay between retries
    double backoffMultiplier = 2.0;     //!< Multiplier for exponential backoff
    bool addJitter = true;              //!< Add random jitter to delays
    double jitterFactor = 0.1;          //!< Jitter factor (0.0 - 1.0)
};

//! Retry policy with exponential backoff
/** Implements a retry policy with configurable exponential backoff for handling
    transient failures in database operations.

    Features:
    - Configurable max attempts
    - Exponential backoff with optional jitter
    - Customizable retryable error detection
    - Statistics tracking

    @par Example:
    @code
    RetryPolicy policy;
    policy.setMaxAttempts(5);
    policy.setInitialDelayMs(100);
    policy.setBackoffMultiplier(2.0);
    policy.setEnabled(true);

    auto result = policy.execute([&]() {
        return db.exec("INSERT INTO ...");
    }, [](const std::string& err) {
        return err.find("connection") != std::string::npos;
    });
    @endcode
*/
class RetryPolicy {
public:
    //! Result of a retry operation
    template<typename T>
    struct Result {
        bool success = false;       //!< Whether the operation succeeded
        T value;                    //!< The result value (if successful)
        int attempts = 0;           //!< Number of attempts made
        std::string lastError;      //!< Last error message (if failed)
    };

    //! Constructor with default configuration
    DLLLOCAL RetryPolicy();

    //! Constructor with custom configuration
    DLLLOCAL explicit RetryPolicy(const RetryConfig& config);

    //! Destructor
    DLLLOCAL ~RetryPolicy() = default;

    //! Check if retry is enabled
    DLLLOCAL bool isEnabled() const { return config.enabled; }

    //! Enable or disable retry
    DLLLOCAL void setEnabled(bool enable) { config.enabled = enable; }

    //! Get maximum number of attempts
    DLLLOCAL int getMaxAttempts() const { return config.maxAttempts; }

    //! Set maximum number of attempts
    DLLLOCAL void setMaxAttempts(int attempts) { config.maxAttempts = attempts; }

    //! Get initial delay in milliseconds
    DLLLOCAL double getInitialDelayMs() const { return config.initialDelayMs; }

    //! Set initial delay in milliseconds
    DLLLOCAL void setInitialDelayMs(double delayMs) { config.initialDelayMs = delayMs; }

    //! Get maximum delay in milliseconds
    DLLLOCAL double getMaxDelayMs() const { return config.maxDelayMs; }

    //! Set maximum delay in milliseconds
    DLLLOCAL void setMaxDelayMs(double delayMs) { config.maxDelayMs = delayMs; }

    //! Get backoff multiplier
    DLLLOCAL double getBackoffMultiplier() const { return config.backoffMultiplier; }

    //! Set backoff multiplier
    DLLLOCAL void setBackoffMultiplier(double multiplier) { config.backoffMultiplier = multiplier; }

    //! Check if jitter is enabled
    DLLLOCAL bool isJitterEnabled() const { return config.addJitter; }

    //! Enable or disable jitter
    DLLLOCAL void setJitterEnabled(bool enable) { config.addJitter = enable; }

    //! Get the current configuration
    DLLLOCAL RetryConfig getConfig() const { return config; }

    //! Set the configuration
    DLLLOCAL void setConfig(const RetryConfig& cfg) { config = cfg; }

    //! Get retry statistics
    DLLLOCAL RetryStats getStats() const { return stats; }

    //! Reset statistics
    DLLLOCAL void resetStats() { stats = RetryStats(); }

    //! Add an ODBC error code that should trigger retry
    /** Common retryable ODBC states:
        - 08S01: Communication link failure
        - 08003: Connection not open
        - HYT00: Timeout expired
        - HYT01: Connection timeout expired

        @param sqlState the 5-character SQLSTATE code
     */
    DLLLOCAL void addRetryableState(const std::string& sqlState);

    //! Check if an error state is retryable
    DLLLOCAL bool isRetryable(const std::string& sqlState) const;

    //! Calculate delay for a given attempt number
    /** @param attempt the attempt number (1-based)
        @return delay in milliseconds
     */
    DLLLOCAL double calculateDelay(int attempt) const;

    //! Execute an operation with retry
    /** @param operation the operation to execute
        @param shouldRetry function to determine if error is retryable
        @return Result containing success status and value or error
     */
    template<typename T>
    Result<T> execute(
        std::function<T()> operation,
        std::function<bool(const std::string&)> shouldRetry = nullptr
    ) {
        Result<T> result;
        result.attempts = 0;

        if (!config.enabled) {
            // If retry is disabled, just execute once
            try {
                result.value = operation();
                result.success = true;
                result.attempts = 1;
                stats.totalAttempts++;
                stats.totalSuccesses++;
            } catch (const std::exception& e) {
                result.success = false;
                result.lastError = e.what();
                result.attempts = 1;
                stats.totalAttempts++;
                stats.totalFailures++;
            }
            return result;
        }

        for (int attempt = 1; attempt <= config.maxAttempts; ++attempt) {
            result.attempts = attempt;
            stats.totalAttempts++;

            try {
                result.value = operation();
                result.success = true;
                stats.totalSuccesses++;
                if (attempt > 1) {
                    stats.totalRetries += (attempt - 1);
                }
                return result;
            } catch (const std::exception& e) {
                result.lastError = e.what();

                // Check if we should retry
                bool canRetry = (attempt < config.maxAttempts);
                if (canRetry && shouldRetry) {
                    canRetry = shouldRetry(result.lastError);
                }

                if (!canRetry) {
                    result.success = false;
                    stats.totalFailures++;
                    return result;
                }

                // Wait before retrying
                double delayMs = calculateDelay(attempt);
                stats.totalWaitTimeMs += delayMs;
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(static_cast<int64_t>(delayMs))
                );
            }
        }

        result.success = false;
        stats.totalFailures++;
        return result;
    }

    //! Execute a void operation with retry
    DLLLOCAL bool executeVoid(
        std::function<void()> operation,
        std::function<bool(const std::string&)> shouldRetry = nullptr,
        std::string* errorOut = nullptr
    );

private:
    //! Configuration
    RetryConfig config;

    //! Statistics
    RetryStats stats;

    //! Set of retryable SQLSTATE codes
    std::vector<std::string> retryableStates;

    //! Random number generator for jitter
    mutable std::mt19937 rng;
};

} // namespace odbc

#endif // _QORE_MODULE_ODBC_RETRYPOLICY_H
