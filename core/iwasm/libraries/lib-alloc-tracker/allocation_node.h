/*
 * Copyright (C) 2024 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef _ALLOCATION_NODE_H
#define _ALLOCATION_NODE_H

#include "bh_hashmap.h"

typedef uint64_t func_idx_t;
typedef uint32_t func_offset_t;

typedef struct {
    size_t size;
    size_t count;
    size_t child_size;
    size_t child_count;
} alloc_stats_t;

typedef struct allocation_node_t {
    struct allocation_node_t *parent;
    HashMap *children;
    size_t children_count;
    func_idx_t func_idx;
    func_offset_t func_offset;
    const char *func_name;

    alloc_stats_t current;
    alloc_stats_t total;
} allocation_node_t;

allocation_node_t *
allocation_node_create(allocation_node_t *parent, const char *name,
                       func_idx_t idx, func_offset_t offset);

void
allocation_node_destroy(void *ptr);

allocation_node_t *
allocation_node_find_or_create(allocation_node_t *parent, const char *name,
                               func_idx_t func_idx, func_offset_t func_offset);

#endif