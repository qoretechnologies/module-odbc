/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    RetryPolicy.cpp

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

#include "RetryPolicy.h"

#include <algorithm>
#include <cmath>

namespace odbc {

RetryPolicy::RetryPolicy()
    : rng(std::random_device{}())
{
    // Add common retryable SQLSTATE codes
    retryableStates.push_back("08S01");  // Communication link failure
    retryableStates.push_back("08003");  // Connection not open
    retryableStates.push_back("08007");  // Connection failure during transaction
    retryableStates.push_back("HYT00");  // Timeout expired
    retryableStates.push_back("HYT01");  // Connection timeout expired
    retryableStates.push_back("40001");  // Serialization failure (deadlock)
}

RetryPolicy::RetryPolicy(const RetryConfig& cfg)
    : config(cfg), rng(std::random_device{}())
{
    // Add common retryable SQLSTATE codes
    retryableStates.push_back("08S01");
    retryableStates.push_back("08003");
    retryableStates.push_back("08007");
    retryableStates.push_back("HYT00");
    retryableStates.push_back("HYT01");
    retryableStates.push_back("40001");
}

void RetryPolicy::addRetryableState(const std::string& sqlState) {
    // Check if already present
    for (const auto& state : retryableStates) {
        if (state == sqlState) {
            return;
        }
    }
    retryableStates.push_back(sqlState);
}

bool RetryPolicy::isRetryable(const std::string& sqlState) const {
    for (const auto& state : retryableStates) {
        if (state == sqlState) {
            return true;
        }
    }
    return false;
}

double RetryPolicy::calculateDelay(int attempt) const {
    if (attempt <= 1) {
        return config.initialDelayMs;
    }

    // Calculate exponential backoff: initialDelay * multiplier^(attempt-1)
    double delay = config.initialDelayMs *
                   std::pow(config.backoffMultiplier, attempt - 1);

    // Cap at maximum delay
    delay = std::min(delay, config.maxDelayMs);

    // Add jitter if enabled
    if (config.addJitter && config.jitterFactor > 0) {
        std::uniform_real_distribution<double> dist(
            1.0 - config.jitterFactor,
            1.0 + config.jitterFactor
        );
        delay *= dist(rng);
    }

    return delay;
}

bool RetryPolicy::executeVoid(
    std::function<void()> operation,
    std::function<bool(const std::string&)> shouldRetry,
    std::string* errorOut
) {
    if (!config.enabled) {
        // If retry is disabled, just execute once
        try {
            operation();
            stats.totalAttempts++;
            stats.totalSuccesses++;
            return true;
        } catch (const std::exception& e) {
            stats.totalAttempts++;
            stats.totalFailures++;
            if (errorOut) {
                *errorOut = e.what();
            }
            return false;
        }
    }

    std::string lastError;

    for (int attempt = 1; attempt <= config.maxAttempts; ++attempt) {
        stats.totalAttempts++;

        try {
            operation();
            stats.totalSuccesses++;
            if (attempt > 1) {
                stats.totalRetries += (attempt - 1);
            }
            return true;
        } catch (const std::exception& e) {
            lastError = e.what();

            // Check if we should retry
            bool canRetry = (attempt < config.maxAttempts);
            if (canRetry && shouldRetry) {
                canRetry = shouldRetry(lastError);
            }

            if (!canRetry) {
                stats.totalFailures++;
                if (errorOut) {
                    *errorOut = lastError;
                }
                return false;
            }

            // Wait before retrying
            double delayMs = calculateDelay(attempt);
            stats.totalWaitTimeMs += delayMs;
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int64_t>(delayMs))
            );
        }
    }

    stats.totalFailures++;
    if (errorOut) {
        *errorOut = lastError;
    }
    return false;
}

} // namespace odbc
