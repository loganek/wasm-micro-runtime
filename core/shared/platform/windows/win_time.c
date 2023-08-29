/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "platform_api_vmcore.h"
#include <winternl.h>



#define NANOSECONDS_PER_SECOND 1000000000

typedef uint32_t __wasi_clockid_t;
#define __WASI_CLOCK_REALTIME (0)
#define __WASI_CLOCK_MONOTONIC (1)
#define __WASI_CLOCK_PROCESS_CPUTIME_ID (2)
#define __WASI_CLOCK_THREAD_CPUTIME_ID (3)

typedef uint64_t __wasi_timestamp_t;

typedef uint16_t __wasi_errno_t ;
#define __WASI_ESUCCESS (0)
#define __WASI_ENOSYS (1)

static uint64
calculate_monotonic_clock_frequency()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return (uint64)frequency.QuadPart;
}

// The implementation below dervies from the following source: https://github.com/WasmEdge/WasmEdge/blob/b70f48c42922ce5ee7730054b6ac0b1615176285/lib/host/wasi/win.h#L210
static __wasi_timestamp_t
from_file_time_to_wasi_timestamp(FILETIME Filetime)
{
    __wasi_timestamp_t *attempt;

    static const uint64 NTToUnixEpoch = 134774 * 86400 * NANOSECONDS_PER_SECOND;

    ULARGE_INTEGER Temp = { .LowPart = Filetime.dwLowDateTime,
                            .HighPart = Filetime.dwHighDateTime };

    auto Duration = Temp.QuadPart - NTToUnixEpoch;

    return Duration;
}

uint64
current_value_of_peformance_counter()
{
    LARGE_INTEGER Counter;
    QueryPerformanceCounter(&Counter);
    return Counter.QuadPart;
}


uint64
os_time_get_boot_microsecond()
{
    struct timespec ts;
#if defined(__MINGW32__)
    // https://www.mail-archive.com/mingw-w64-public@lists.sourceforge.net/msg18361.html
    clock_gettime(CLOCK_REALTIME, &ts);
#else
    timespec_get(&ts, TIME_UTC);
#endif

    return ((uint64)ts.tv_sec) * 1000 * 1000 + ((uint64)ts.tv_nsec) / 1000;
}

uint64
os_win_clock_res_get(__wasi_clockid_t clock_id, __wasi_timestamp_t *resolution)
{
    switch (clock_id) {
        case __WASI_CLOCK_MONOTONIC:
        {
             const __wasi_timestamp_t result =
                (uint64_t)(((uint64_t)1000000000)
                           / current_value_of_peformance_counter());
            *resolution = (__wasi_timestamp_t)result;
            __wasi_errno_t error = 0;
            return __WASI_ESUCCESS;
            
        }
        case __WASI_CLOCK_REALTIME:
        case __WASI_CLOCK_PROCESS_CPUTIME_ID:
        case __WASI_CLOCK_THREAD_CPUTIME_ID:
        {
            PULONG MaximumTime;
            PULONG MinimumTime;
            PULONG CurrentTime;
            NTSTATUS
            NTAPI Res;
            if (Res = NtQueryTimerResolution(&MaximumTime, &MinimumTime,
                                             &CurrentTime))
                ;
                !NT_SUCCESS(Res);
            {
                return -1;
            }
        }
    }
    

}
uint64
os_win_clock_time_get(__wasi_clockid_t clock_id, __wasi_timestamp_t precision,
                     __wasi_timestamp_t *time)
{
    switch (clock_id) {

        case __WASI_CLOCK_REALTIME:
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

        case __WASI_CLOCK_MONOTONIC:
        {
            uint64_t Nanoseconds;
            const auto Counter = calculate_monotonic_clock_frequency();
            if (NANOSECONDS_PER_SECOND
                           % calculate_monotonic_clock_frequency()
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

            return __WASI_ESUCCESS;

            case __WASI_CLOCK_PROCESS_CPUTIME_ID:
            {
                FILETIME CreationTime;
                FILETIME ExitTime;
                FILETIME KernalTime;
                FILETIME UserTime;

                if (!GetProcessTimes(GetCurrentProcess(),
                                              &CreationTime, &ExitTime,
                                              &KernalTime, &UserTime)) {

                    return _get_errno(__WASI_ENOSYS); 
                }
                time = from_file_time_to_wasi_timestamp(KernalTime)
                       + from_file_time_to_wasi_timestamp(UserTime);

               return __WASI_ESUCCESS;
            }

            case __WASI_CLOCK_THREAD_CPUTIME_ID:
            {
                FILETIME CreationTime;
                FILETIME ExitTime;
                FILETIME KernalTime;
                FILETIME UserTime;

                if (!GetProcessTimes(GetCurrentProcess(),
                                              &CreationTime, &ExitTime,
                                              &KernalTime, &UserTime)) {
                    
                return _get_errno(__WASI_ENOSYS); 
                    
                }

                time = from_file_time_to_wasi_timestamp(KernalTime)
                       + from_file_time_to_wasi_timestamp(UserTime);

                return __WASI_ESUCCESS;
             
            }
        }
    }

}