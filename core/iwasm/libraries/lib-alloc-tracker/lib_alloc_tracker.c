/*
 * Copyright (C) 2024 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "bh_log.h"
#include "allocation_node.h"
#include "massif_output.h"
#include "wasm_runtime.h"

#if WASM_ENABLE_INTERP != 0
#include "wasm_runtime.h"
#endif

#if WASM_ENABLE_AOT != 0
#include "aot_runtime.h"
#endif

#if WASM_ENABLE_THREAD_MGR != 0
static korp_mutex _lock;
#endif

typedef struct {
    size_t size;
    allocation_node_t *node;
} allocation_info_t;

typedef struct {
    massif_context_t *massif_ctx;
    allocation_node_t *root_node;
    HashMap *allocations;
    size_t total_size;
} alloc_tracker_context_t;

static alloc_tracker_context_t _context;

static uint32
_allocation_hash(const void *key)
{
    return (uint32)(uintptr_t)key;
}

static bool
_allocation_equal(void *key1, void *key2)
{
    return key1 == key2;
}

static char *
_generate_name(char *buf, size_t buf_size)
{
    char *env_name = getenv("ALLOC_TRACKER_MASSIF_OUTPUT");
    if (env_name) {
        return env_name;
    }
    snprintf(buf, buf_size, "wamr.massif.%d", (int)getpid());
    return buf;
}

bool
_init_context(alloc_tracker_context_t *ctx)
{
    char name_buf[128];

    memset(ctx, 0, sizeof(alloc_tracker_context_t));
    ctx->root_node = allocation_node_create(NULL, NULL, 0, 0);
    if (!ctx->root_node) {
        goto fail;
    }
    ctx->allocations =
        bh_hash_map_create(1024, false, _allocation_hash, _allocation_equal,
                           NULL, wasm_runtime_free);
    if (!ctx->allocations) {
        goto fail;
    }
    ctx->massif_ctx = massif_context_create(
        _generate_name(name_buf, sizeof(name_buf) / sizeof(name_buf[0])),
        ctx->root_node);
    if (!ctx->massif_ctx) {
        goto fail;
    }
    ctx->total_size = 0;
    return true;

fail:
    if (ctx->root_node) {
        allocation_node_destroy(ctx->root_node);
    }
    if (ctx->allocations) {
        bh_hash_map_destroy(ctx->allocations);
    }
    return false;
}

void
_deinit_context(alloc_tracker_context_t *ctx)
{
    allocation_node_destroy(ctx->root_node);
    bh_hash_map_destroy(ctx->allocations);
}

static Vector *
prepare(wasm_exec_env_t exec_env)
{
    WASMModuleInstance *module_inst =
        (WASMModuleInstance *)get_module_inst(exec_env);

#if WASM_ENABLE_INTERP != 0
    if (module_inst->module_type == Wasm_Module_Bytecode
        && wasm_interp_create_call_stack(exec_env)) {
    }
#endif

#if WASM_ENABLE_AOT != 0
    if (module_inst->module_type == Wasm_Module_AoT
        && aot_create_call_stack(exec_env)) {
    }
#endif

    return module_inst->frames;
}

static void
_capture_allocation(wasm_exec_env_t exec_env, void *ptr, size_t size)
{
    allocation_node_t *start = _context.root_node;
    Vector *frames = prepare(exec_env);

#if WASM_ENABLE_THREAD_MGR != 0
    os_mutex_lock(&_lock);
#endif

    _context.total_size += size;

    uint32 total_frames = (uint32)bh_vector_size(frames);
    uint32 n = 1;
    allocation_info_t *info =
        (allocation_info_t *)wasm_runtime_malloc(sizeof(allocation_info_t));

    if (!info) {
        LOG_ERROR("allocation failed");
        goto finish;
    }

    info->size = size;

    while (n < total_frames) {
        WASMCApiFrame frame = { 0 };
        if (!bh_vector_get(frames, n, &frame)) {
            LOG_ERROR("cannot read stack frame");
            goto finish;
        }

        start = allocation_node_find_or_create(
            start, frame.func_name_wp, frame.func_index, frame.func_offset);
        if (n == 1) {
            start->current.size += size;
            start->current.count++;
            start->total.size += size;
            start->total.count++;
        }
        start->current.child_size += size;
        start->current.child_count++;
        start->total.child_size += size;
        start->total.child_count++;
        n++;
    }

    info->node = start;
    if (!bh_hash_map_insert(_context.allocations, ptr, info)) {
        LOG_ERROR("insert info to hashmap failed");
        wasm_runtime_free(info);
        goto finish;
    }

    massif_context_snapshot(_context.massif_ctx, _context.total_size, true);

finish:
#if WASM_ENABLE_THREAD_MGR != 0
    os_mutex_unlock(&_lock);
#endif
}

static void
_capture_free(void *ptr)
{
#if WASM_ENABLE_THREAD_MGR != 0
    os_mutex_lock(&_lock);
#endif

    allocation_info_t *value =
        (allocation_info_t *)bh_hash_map_find(_context.allocations, ptr);
    if (value == NULL) {
        LOG_ERROR("Failed to find pointer %p\n", ptr);
        goto finish;
    }
    bh_hash_map_remove(_context.allocations, ptr, NULL, NULL);
    _context.total_size -= value->size;
    allocation_node_t *cur = value->node;
    while (cur->parent) {
        cur->current.child_size -= value->size;
        cur->current.child_count--;
        cur = cur->parent;
    }

    massif_context_snapshot(_context.massif_ctx, _context.total_size, true);

finish:
#if WASM_ENABLE_THREAD_MGR != 0
    os_mutex_unlock(&_lock);
#endif
}

static void
trace_malloc(wasm_exec_env_t exec_env, void *ret, int32_t size)
{
    _capture_allocation(exec_env, ret, size);
}

static void
trace_calloc(wasm_exec_env_t exec_env, void *ret, int32_t count, int32_t size)
{
    _capture_allocation(exec_env, ret, count * size);
}

static void
trace_realloc(wasm_exec_env_t exec_env, void *ret, void *ptr, int32_t size)
{
    _capture_free(ptr); // TODO optimize for ret == ptr
    _capture_allocation(exec_env, ret, size);
}

static void
trace_free(wasm_exec_env_t exec_env, void *ptr)
{
    _capture_free(ptr);
}

static NativeSymbol native_symbols_lib_alloc_tracker[] = {
    { "trace_malloc", trace_malloc, "(*i)", NULL },
    { "trace_calloc", trace_calloc, "(*ii)", NULL },
    { "trace_realloc", trace_realloc, "(**i)", NULL },
    { "trace_free", trace_free, "(*)", NULL }
};

uint32
get_lib_alloc_tracker_export_apis(NativeSymbol **p_lib_alloc_tracker_apis)
{
    *p_lib_alloc_tracker_apis = native_symbols_lib_alloc_tracker;
    return sizeof(native_symbols_lib_alloc_tracker) / sizeof(NativeSymbol);
}

bool
lib_alloc_tracker_init(void)
{
#if WASM_ENABLE_THREAD_MGR != 0
    if (0 != os_mutex_init(&_lock))
        return false;
#endif

    if (!_init_context(&_context)) {
#if WASM_ENABLE_THREAD_MGR != 0
        os_mutex_destroy(&_lock);
#endif
        return false;
    }

    return true;
}

void
lib_alloc_tracker_destroy(void)
{
#if WASM_ENABLE_THREAD_MGR != 0
    os_mutex_destroy(&_lock);
#endif
    _deinit_context(&_context);
}
