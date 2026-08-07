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

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "utils/logger.h"
#include "utils/timer.h"

static bool g_perf_profiler_enabled = false;

void perf_profiler_set_enabled(bool enabled)
{
    g_perf_profiler_enabled = enabled;
}

bool perf_profiler_is_enabled(void)
{
    return g_perf_profiler_enabled;
}

PerfTimer perf_timer_start(const char *name)
{
    PerfTimer timer;
    timer.name = name ? name : "unnamed_operation";
    timer.running = true;
    clock_gettime(CLOCK_MONOTONIC, &timer.start_ts);
    timer.stop_ts = timer.start_ts;
    return timer;
}

double perf_timer_stop(PerfTimer *timer)
{
    if (!timer) {
        return 0.0;
    }
    if (timer->running) {
        clock_gettime(CLOCK_MONOTONIC, &timer->stop_ts);
        timer->running = false;
    }
    return perf_timer_elapsed_ms(timer);
}

double perf_timer_elapsed_us(const PerfTimer *timer)
{
    if (!timer) {
        return 0.0;
    }
    struct timespec end_ts = timer->stop_ts;
    if (timer->running) {
        clock_gettime(CLOCK_MONOTONIC, &end_ts);
    }
    double seconds = (double)(end_ts.tv_sec - timer->start_ts.tv_sec);
    double nanoseconds = (double)(end_ts.tv_nsec - timer->start_ts.tv_nsec);
    return (seconds * 1000000.0) + (nanoseconds / 1000.0);
}

double perf_timer_elapsed_ms(const PerfTimer *timer)
{
    return perf_timer_elapsed_us(timer) / 1000.0;
}

void perf_timer_log(const PerfTimer *timer)
{
    if (!timer || !g_perf_profiler_enabled) {
        return;
    }
    perf_timer_log_force(timer);
}

void perf_timer_log_force(const PerfTimer *timer)
{
    if (!timer) {
        return;
    }
    double us = perf_timer_elapsed_us(timer);
    double ms = us / 1000.0;
    if (ms >= 1.0) {
        log_info("[PERF] %s completed in %.2f ms (%.0f us)", timer->name, ms, us);
    } else {
        log_info("[PERF] %s completed in %.0f us (%.3f ms)", timer->name, us, ms);
    }
}
