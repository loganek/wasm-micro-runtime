/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "platform_api_vmcore.h"
#include <time.h>

static __wasi_timestamp_t
convert_timespec(const struct timespec *ts)
{
    if (ts->tv_sec < 0)
        return 0;
    if ((__wasi_timestamp_t)ts->tv_sec >= UINT64_MAX / 1000000000)
        return UINT64_MAX;
    return (__wasi_timestamp_t)ts->tv_sec * 1000000000
           + (__wasi_timestamp_t)ts->tv_nsec;
}

static bool
convert_clockid(__wasi_clockid_t in, clockid_t *out)
{
    switch (in) {
        case __WASI_CLOCK_MONOTONIC:
            *out = CLOCK_MONOTONIC;
            return true;
#if defined(CLOCK_PROCESS_CPUTIME_ID)
        case __WASI_CLOCK_PROCESS_CPUTIME_ID:
            *out = CLOCK_PROCESS_CPUTIME_ID;
            return true;
#endif
        case __WASI_CLOCK_REALTIME:
            *out = CLOCK_REALTIME;
            return true;
#if defined(CLOCK_THREAD_CPUTIME_ID)
        case __WASI_CLOCK_THREAD_CPUTIME_ID:
            *out = CLOCK_THREAD_CPUTIME_ID;
            return true;
#endif
        default:
            return false;
    }
}
#endif

uint64
os_time_get_boot_microsecond()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }

    return ((uint64)ts.tv_sec) * 1000 * 1000 + ((uint64)ts.tv_nsec) / 1000;
}

uint64
os_clock_res_get(bh_clock_id_t clock_id, uint64 *resolution)
{
    bh_clock_id_t nclock_id;
    if (!convert_clockid(clock_id, &nclock_id))
        return BHT_ERROR;
    struct timespec ts;
    if (clock_getres(clock_id, &ts) < 0)
        return BHT_ERROR;
    *resolution = convert_timespec(&ts);

}

uint64
os_clock_time_get(bh_clock_id_t clock_id, uint64 precision, uint64 *time)
{
    bh_clock_id_t nclock_id;
    if (!convert_clockid(clock_id, &nclock_id))
        return __WASI_EINVAL;
        struct timespec ts;
    if (clock_gettime(nclock_id, &ts) < 0)
        return BHT_ERROR;
        *time = convert_timespec(&ts);
}
