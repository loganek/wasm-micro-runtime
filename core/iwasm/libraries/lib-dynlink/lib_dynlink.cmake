# Copyright (C) 2024 Amazon.com Inc. or its affiliates. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set (LIB_DYNLINK_DIR ${CMAKE_CURRENT_LIST_DIR})

include_directories(${LIB_DYNLINK_DIR})

file (GLOB source_all ${LIB_DYNLINK_DIR}/*.c)

set (LIB_DYNLINK_SOURCE ${source_all})

