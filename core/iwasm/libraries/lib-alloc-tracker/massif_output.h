/*
 * Copyright (C) 2024 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef _MASSIF_OUTPUT_H
#define _MASSIF_OUTPUT_H

#include "allocation_node.h"

typedef struct massif_context_t massif_context_t;

massif_context_t *
massif_context_create(const char *path, allocation_node_t *root);

void
massif_context_destroy(massif_context_t *context);

void
massif_context_snapshot(massif_context_t *context, size_t total_size,
                        bool detailed_snapshot);

#endif