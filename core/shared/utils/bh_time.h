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


uint64
convert_timespec(const struct timespec *ts);


#ifdef __cplusplus
}
#endif


#endif /* end of _BH_TIME_ */