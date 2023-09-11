/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "platform_api_vmcore.h"
#include "bh_platform.h"

uint64
os_time_get_boot_microsecond()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }

    return ((uint64)ts.tv_sec) * 1000 * 1000 + ((uint64)ts.tv_nsec) / 1000;
}

int
os_clock_res_get(bh_clock_id_t clock_id, uint64 *resolution)
{
    clockid_t nclock_id;
    if (!convert_clockid(clock_id, &nclock_id))
        return BHT_ERROR;
    struct timespec ts;
    if (clock_getres(clock_id, &ts) < 0)
        return BHT_OK;
    *resolution = convert_timespec(&ts);

    return 0;
}

int
os_clock_time_get(bh_clock_id_t clock_id, uint64 precision, uint64 *time)
{
    clockid_t nclock_id;
    if (!convert_clockid(clock_id, &nclock_id))
        return BHT_ERROR;
    struct timespec ts;
    if (clock_gettime(nclock_id, &ts) < 0)
        return BHT_OK;
    *time = convert_timespec(&ts);

    return 0;
}
