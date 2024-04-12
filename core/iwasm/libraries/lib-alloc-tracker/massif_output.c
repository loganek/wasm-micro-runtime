/*
 * Copyright (C) 2024 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */
#include "massif_output.h"
#include "bh_assert.h"

#include <stdlib.h>

struct massif_context_t {
    allocation_node_t *root;
    FILE *stream;
    int snapshot_index;
};

typedef struct {
    size_t size;
    int level;
    massif_context_t *ctx;
} user_data_t;

void
_massif_dump(massif_context_t *ctx, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(ctx->stream, format, args);
    va_end(args);
}

static void
_print_snapshot_tree(void *key, void *value, void *user_data)
{
    allocation_node_t *node = (allocation_node_t *)value;
    user_data_t *ud = (user_data_t *)user_data;
    for (int i = 0; i < ud->level; i++) {
        _massif_dump(ud->ctx, " ");
    }
    _massif_dump(ud->ctx, "n%zu: %zu 0x0: ", node->children_count,
                 node->current.child_size);
    if (node->func_name) {
        _massif_dump(ud->ctx, "%s", node->func_name);
    }
    else {
        _massif_dump(ud->ctx, "%" PRIu64, node->func_idx);
    }
    _massif_dump(ud->ctx, "+%" PRIu32 "\n", node->func_offset);
    if (node->children) {
        ud->level++;
        bh_hash_map_traverse(node->children, _print_snapshot_tree, ud);
        ud->level--;
    }
}

massif_context_t *
massif_context_create(const char *path, allocation_node_t *root)
{
    massif_context_t *ctx;

    bh_assert(root);
    bh_assert(path);

    ctx = (massif_context_t *)wasm_runtime_malloc(sizeof(massif_context_t));
    if (!ctx) {
        return NULL;
    }
    ctx->stream = fopen(path, "w");
    if (!ctx->stream) {
        goto fail;
    }

    ctx->root = root;
    ctx->snapshot_index = 0;

    return ctx;

fail:
    if (ctx) {
        wasm_runtime_free(ctx);
    }

    return NULL;
}

void
massif_context_destroy(massif_context_t *context);

void
massif_context_snapshot(massif_context_t *context, size_t total_size,
                        bool detailed_snapshot);

void
massif_context_snapshot(massif_context_t *context, size_t total_size,
                        bool detailed_snapshot)
{
    if (context->snapshot_index == 0) {
        _massif_dump(context, "desc: nope\n");     // TODO
        _massif_dump(context, "cmd: test.wasm\n"); // TODO
        _massif_dump(context, "time_unit: ms\n");
    }

    _massif_dump(context, "#-----------\n");
    _massif_dump(context, "snapshot=%d\n", context->snapshot_index);
    _massif_dump(context, "#-----------\n");
    _massif_dump(context, "time=%lu\n",
                 os_time_get_boot_us()
                     / 1000); // TODO realtime clock instead of monotonic?
    _massif_dump(context, "mem_heap_B=%zu\n", total_size);
    _massif_dump(context, "mem_heap_extra_B=0\n"); // TODO include that value
    _massif_dump(context, "mem_stacks_B=0\n");
    _massif_dump(context, "heap_tree=%s\n",
                 detailed_snapshot ? "detailed" : "empty");
    if (detailed_snapshot) {
        user_data_t ud = { 0, 0, context };
        _print_snapshot_tree(NULL, context->root, &ud);
    }
    context->snapshot_index++;
}
