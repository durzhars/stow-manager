/*
 * Dotfiles Stow Manager (stow-manager)
 * Copyright (C) 2026 durzhars
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef UTILS_TIMER_H
#define UTILS_TIMER_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    struct timespec start_ts;
    struct timespec stop_ts;
    const char *name;
    bool running;
} PerfTimer;

// Enable/disable performance profiling log outputs globally
void perf_profiler_set_enabled(bool enabled);
bool perf_profiler_is_enabled(void);

// Start a timer with a given label
PerfTimer perf_timer_start(const char *name);

// Stop the timer and record end timestamp (returns elapsed ms)
double perf_timer_stop(PerfTimer *timer);

// Returns elapsed time in milliseconds (ms)
double perf_timer_elapsed_ms(const PerfTimer *timer);

// Returns elapsed time in microseconds (us)
double perf_timer_elapsed_us(const PerfTimer *timer);

// Logs the timer's elapsed duration if profiling is enabled (or forcibly if force_log is true)
void perf_timer_log(const PerfTimer *timer);
void perf_timer_log_force(const PerfTimer *timer);

// Convenience macros for scope profiling
#define PERF_PROFILE_START(label) PerfTimer _timer_##label = perf_timer_start(#label)
#define PERF_PROFILE_END(label) perf_timer_log(&_timer_##label)

#endif /* UTILS_TIMER_H */
