/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "platform_api_vmcore.h"
#include <winternl.h>

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
    #ifdef BH_PLATFORM_WINDOWS 
    switch (clock_id) {
        case BH_CLOCK_MONOTONIC:
        {
            const uint64 result = (uint64)NANOSECONDS_PER_SECOND
                                  / calculate_monotonic_clock_frequency();
            *resolution = result;
            return BHT_OK;
        }
        case BH_CLOCK_REALTIME:
        case BH_CLOCK_PROCESS_CPUTIME_ID:
        case BH_CLOCK_THREAD_CPUTIME_ID:
        {
            PULONG MaximumTime;
            PULONG MinimumTime;
            PULONG CurrentTime;
            NTSTATUS
            NTAPI Res;
            if (Res = NtQueryTimerResolution(&MaximumTime, &MinimumTime,
                                             &CurrentTime)) {
                !NT_SUCCESS(Res);
            }
            else

            {
                return NT_ERROR(Res);
            }
        }
    }

#else {
    __wasi_clockid_t nclock_id;
        if (!convert_clockid(clock_id, &nclock_id))
            return __WASI_EINVAL;
        struct timespec ts;
        if (clock_getres(nclock_id, &ts) < 0)
            return convert_errno(errno);
        *resolution = convert_timespec(&ts);
 }
#endif
}

uint64
os_clock_time_get(bh_clock_id_t clock_id, uint64 precision, uint64 *time)
{
    #ifdef BH_PLATFORM_WINDOWS 
    switch (clock_id) {

        case BH_CLOCK_REALTIME:
        {
            FILETIME SysNow;
#if NTDDI_VERSION >= NTDDI_WIN8
            GetSystemTimePreciseAsFileTime(&SysNow);
#else
            GetSystemTimeAsFileTime(&SysNow);

#endif
            time = from_file_time_to_wasi_timestamp(SysNow);
            return 0;
        }

        case BH_CLOCK_MONOTONIC:
        {
            uint64_t Nanoseconds;
            const auto Counter = calculate_monotonic_clock_frequency();
            if (NANOSECONDS_PER_SECOND % calculate_monotonic_clock_frequency()
                == 0) {
                Nanoseconds = Counter
                              * (NANOSECONDS_PER_SECOND
                                 / calculate_monotonic_clock_frequency());
            }
            else {
                const auto Seconds =
                    Counter / calculate_monotonic_clock_frequency();
                const auto Fractions =
                    Counter % calculate_monotonic_clock_frequency();
                Nanoseconds = Seconds * NANOSECONDS_PER_SECOND
                              + (Fractions * NANOSECONDS_PER_SECOND)
                                    / calculate_monotonic_clock_frequency();
            }
            *time = Nanoseconds;

            return BHT_OK;

            case BH_CLOCK_PROCESS_CPUTIME_ID:
            {
                FILETIME CreationTime;
                FILETIME ExitTime;
                FILETIME KernalTime;
                FILETIME UserTime;

                if (!GetProcessTimes(GetCurrentProcess(), &CreationTime,
                                     &ExitTime, &KernalTime, &UserTime)) {

                    return _get_errno(__WASI_ENOSYS);
                }
                time = from_file_time_to_wasi_timestamp(KernalTime)
                       + from_file_time_to_wasi_timestamp(UserTime);

                return BHT_OK;
            }

            case BH_CLOCK_THREAD_CPUTIME_ID:
            {
                FILETIME CreationTime;
                FILETIME ExitTime;
                FILETIME KernalTime;
                FILETIME UserTime;

                if (!GetProcessTimes(GetCurrentProcess(), &CreationTime,
                                     &ExitTime, &KernalTime, &UserTime)) {

                    return BHT_ERROR;
                }

                time = from_file_time_to_wasi_timestamp(KernalTime)
                       + from_file_time_to_wasi_timestamp(UserTime);

                return BHT_ERROR;
            }
        }
    }

#else
    {
        clockid_t nclock_id;
        if (!convert_clockid(clock_id, &nclock_id))
            return __WASI_EINVAL;
        struct timespec ts;
        if (clock_gettime(nclock_id, &ts) < 0)
            return convert_errno(errno);
        *time = convert_timespec(&ts);
    }
#endif   
}
