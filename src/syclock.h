/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <chrono>
#include <atomic>
#include <memory>
#include <QMetaType>

#include "eigenaux.h"

Q_DECLARE_METATYPE(std::chrono::milliseconds);
Q_DECLARE_METATYPE(std::chrono::microseconds);

namespace Syntalos {

/**
 * @brief The amount of time a secondary clock is allowed to deviate from the master.
 *
 * Since Syntalos uses millisecond time resolution, permitting (slightly more than)
 * half a millisecond deviation for secondary clocks from the master clock is sensible.
 *
 * IMPORTANT: Modules may override this value for their synchronizers to one that fits their
 * device better. This is just a default for modules which do not change the default setting.
 */
static constexpr auto SECONDARY_CLOCK_TOLERANCE = std::chrono::microseconds(600);

/**
 * @brief Interval at which we check for external clock synchronization
 *
 * IMPORTANT: This is just a default value for modules which do not explicitly define a check
 * interval. Individual modules may choose a different value that fits the device they are
 * communicating with best.
 */
static constexpr auto DEFAULT_CLOCKSYNC_CHECK_INTERVAL = std::chrono::seconds(4);

/**
 * @brief Syntalos Master Clock
 *
 * The master clock that is used as reference for all other connected
 * and device-specific clocks.
 * It is always guaranteed to use monotonic time, only increasing at a
 * uniform rate, and should have nanosecond accuracy.
 *
 * This clock exists so we can be independent of the C++ standard library
 * and exactly control and adjust our clock, as well as experiment with new
 * types of clocks.
*/
struct symaster_clock
{
  typedef std::chrono::nanoseconds  duration;
  typedef duration::rep             rep;
  typedef duration::period          period;
  typedef std::chrono::time_point<symaster_clock, duration> 	time_point;

  static constexpr bool is_steady = true;

  static time_point
  now() noexcept;
};

/// A timepoint on the master clock
using symaster_timepoint = std::chrono::time_point<symaster_clock>;

/// Shorthand for milliseconds
using milliseconds_t = std::chrono::milliseconds;

/// Shorthand for microseconds
using microseconds_t = std::chrono::microseconds;

inline milliseconds_t timeDiffMsec(const symaster_timepoint &timePoint1, const symaster_timepoint &timePoint2) noexcept
{
    return std::chrono::duration_cast<milliseconds_t>(timePoint1 - timePoint2);
}

inline milliseconds_t timeDiffToNowMsec(const std::chrono::time_point<symaster_clock>& timePoint) noexcept
{
    return std::chrono::duration_cast<milliseconds_t>(symaster_clock::now() - timePoint);
}

inline symaster_timepoint currentTimePoint() noexcept
{
    return symaster_clock::now();
}

class SyncTimer
{
public:
    explicit SyncTimer();

    void start() noexcept;
    void startAt(const symaster_timepoint &startTimePoint) noexcept;

    inline milliseconds_t timeSinceStartMsec() noexcept
    {
        return std::chrono::duration_cast<milliseconds_t>(symaster_clock::now() - m_startTime);
    }

    inline std::chrono::nanoseconds timeSinceStartNsec() noexcept
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(symaster_clock::now() - m_startTime);
    }

    inline symaster_timepoint currentTimePoint() noexcept
    {
        return symaster_clock::now();
    }

    symaster_timepoint startTime() noexcept
    {
        return m_startTime;
    }

private:
    symaster_timepoint m_startTime;
    bool m_started;
};

/**
 * Compute a timestamp for "when this function acquired a value".
 * This is assumed to be the mean between function start and end time,
 * rounded to milliseconds.
 * For example, if the function F acquires a timestamp'ed value,
 * this macro should return the equivalent timestamp on our timer T.
 * This should balance out context switches if they are not too bad,
 * and produce a reasonably accurate result in milliseconds.
 * It is superior to measuring our timestamp for alignment after the other
 * timestamping function was run.
 */
#define TIMER_FUNC_TIMESTAMP(T, F) ({ \
    auto __stime = T->timeSinceStartNsec(); \
    F; \
    std::chrono::round<milliseconds_t>((__stime + T->timeSinceStartNsec()) / 2.0); \
    })
#define MTIMER_FUNC_TIMESTAMP(F) ({TIMER_TIMESTAMP_FUNC(m_syTimer, F)})

/**
 * Compute a timestamp for "when this function acquired a value".
 * This function is equivalent to TIMER_FUNC_TIMESTAMP(), but takes
 * a starting timepoint instead of a timer as first parameter, and
 * also captures the function result in FR.
 */
#define FUNC_EXEC_TIMESTAMP(INIT_TIME, F) ({ \
    auto __stime = std::chrono::duration_cast<std::chrono::nanoseconds>(symaster_clock::now() - (INIT_TIME)); \
    F; \
    std::chrono::round<milliseconds_t>((__stime + std::chrono::duration_cast<std::chrono::nanoseconds>(symaster_clock::now() - (INIT_TIME))) / 2.0); \
    })

} // end of namespace
