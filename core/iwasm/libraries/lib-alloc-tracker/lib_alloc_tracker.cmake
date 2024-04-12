# Copyright (C) 2024 Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set (LIB_ALLOC_TRACKER_DIR ${CMAKE_CURRENT_LIST_DIR})

include_directories(${LIB_ALLOC_TRACKER_DIR})
add_definitions (-DWASM_ENABLE_LIB_ALLOC_TRACKER=1)

file (GLOB source_all ${LIB_ALLOC_TRACKER_DIR}/*.c)

set (LIB_ALLOC_TRACKER_SOURCE ${source_all})
