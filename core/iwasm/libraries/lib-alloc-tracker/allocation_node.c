#include "allocation_node.h"

#include "bh_log.h"

static uint32
allocation_node_hash(const void *key)
{
    return (uint32)(uintptr_t)key;
}

static bool
allocation_node_equal(void *key1, void *key2)
{
    return key1 == key2;
}

void
allocation_node_destroy(void *ptr)
{
    allocation_node_t *node = (allocation_node_t *)ptr;
    bh_hash_map_destroy(node->children);
    wasm_runtime_free(node);
}

allocation_node_t *
allocation_node_create(allocation_node_t *parent, const char *name,
                       func_idx_t idx, func_offset_t offset)
{
    allocation_node_t *node =
        (allocation_node_t *)wasm_runtime_malloc(sizeof(allocation_node_t));
    if (!node) {
        LOG_ERROR("Failed to create allocation node.\n");
        return NULL;
    }

    memset(node, 0, sizeof(allocation_node_t));
    node->parent = parent;
    node->func_idx = idx;
    node->func_offset = offset;
    node->func_name = name;
    node->children = bh_hash_map_create(1024, false, allocation_node_hash,
                                        allocation_node_equal, NULL,
                                        allocation_node_destroy);
    if (!node->children) {
        LOG_ERROR("Failed to create allocation node map");
        wasm_runtime_free(node);
        return NULL;
    }
    return node;
}

allocation_node_t *
allocation_node_find_or_create(allocation_node_t *parent, const char *name,
                               func_idx_t func_idx, func_offset_t func_offset)
{
    size_t hashed_key = func_idx + func_offset; // TODO!!!
    allocation_node_t *node =
        bh_hash_map_find(parent->children, (void *)hashed_key);
    if (node == NULL) {
        node = allocation_node_create(parent, name, func_idx, func_offset);
        bh_hash_map_insert(parent->children, (void *)hashed_key, node);
        parent->children_count++;
        return node;
    }
    else {
        return node;
    }
}
