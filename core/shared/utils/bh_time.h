/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef _BH_TIME_
#define _BH_TIME_
#include "bh_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

	
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3

uint64
convert_timespec(const struct timespec *ts);

static bool
convert_clockid(bh_clock_id_t in, clockid_t *out);

#ifdef __cplusplus
}
#endif


#endif /* end of _BH_TIME_ */