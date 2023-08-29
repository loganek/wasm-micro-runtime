/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "platform_api_vmcore.h"

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
os_clock_res_get()
{
    __wasi_clockid_t nclock_id;
    if (!convert_clockid(clock_id, &nclock_id))
        return __WASI_EINVAL;
    struct timespec ts;
    if (clock_getres(nclock_id, &ts) < 0)
        return convert_errno(errno);
    *resolution = convert_timespec(&ts);
  
}

uint64
os_clock_time_get()
{

    clockid_t nclock_id;
    if (!convert_clockid(clock_id, &nclock_id))
        return __WASI_EINVAL;
    struct timespec ts;
    if (clock_gettime(nclock_id, &ts) < 0)
        return convert_errno(errno);
    *time = convert_timespec(&ts);
}