#include "dynlink_loader.h"
#include "wasm_runtime.h"

#include "wasm_leb128.h"

enum DynLinkSubSectionType {
    WASM_DYLINK_MEM_INFO = 1,
    WASM_DYLINK_NEEDED,
    WASM_DYLINK_EXPORT_INFO,
    WASM_DYLINK_IMPORT_INFO,
};

typedef struct {
    uint8 *ptr;
    const uint8 *ptr_end;
    WASMModule *module;
    bool is_load_from_file_buf;
    char *error_buf;
    uint32 error_buf_size;
} DynLinkLoaderCtx;

#undef read_leb_uint32
#define read_leb_uint32(p, p_end, res)                                        \
    do {                                                                      \
        uint64 res64;                                                         \
        if (!read_leb((uint8 **)&p, p_end, 32, false, &res64, ctx->error_buf, \
                      ctx->error_buf_size))                                   \
            goto fail;                                                        \
        res = (uint32)res64;                                                  \
    } while (0)

static void
set_error_buf(const DynLinkLoaderCtx *ctx, const char *string)
{
    if (ctx->error_buf != NULL) {
        snprintf(ctx->error_buf, ctx->error_buf_size,
                 "Loading dynlink.0 section failed: %s", string);
    }
}

static bool
check_buf(const uint8 *buf, const uint8 *buf_end, uint32 length,
          const DynLinkLoaderCtx *ctx)
{
    if ((uintptr_t)buf + length < (uintptr_t)buf
        || (uintptr_t)buf + length > (uintptr_t)buf_end) {
        set_error_buf(ctx, "unexpected end of section or function");
        return false;
    }
    return true;
}

#define CHECK_BUF(buf, buf_end, length)              \
    do {                                             \
        if (!check_buf(buf, buf_end, length, ctx)) { \
            goto fail;                               \
        }                                            \
    } while (0)

static char *
load_string(DynLinkLoaderCtx *ctx)
{
    uint32 length;
    char *ret = NULL;
    bh_assert(ctx);

    read_leb_uint32(ctx->ptr, ctx->ptr_end, length);
    CHECK_BUF(ctx->ptr, ctx->ptr_end, length);
    ret = wasm_const_str_list_insert(ctx->ptr, length, ctx->module,
                                     ctx->is_load_from_file_buf, ctx->error_buf,
                                     ctx->error_buf_size);

    if (ret != NULL) {
        ctx->ptr += length;
    }

fail:
    return ret;
}

static bool
load_mem_info_section(DynLinkLoaderCtx *ctx)
{
    DynLinkSectionMemInfo *mem_info = &ctx->module->dynlink_data.mem_info;

    bh_assert(ctx);

    read_leb_uint32(ctx->ptr, ctx->ptr_end, mem_info->memory_size);
    read_leb_uint32(ctx->ptr, ctx->ptr_end, mem_info->memory_alignment);
    read_leb_uint32(ctx->ptr, ctx->ptr_end, mem_info->table_size);
    read_leb_uint32(ctx->ptr, ctx->ptr_end, mem_info->table_alignment);

    LOG_VERBOSE("Load dylink memory section success:");
    LOG_VERBOSE("  memory size: %" PRIu32, mem_info->memory_size);
    LOG_VERBOSE("  memory alignment: %" PRIu32, mem_info->memory_alignment);
    LOG_VERBOSE("  table size: %" PRIu32, mem_info->table_size);
    LOG_VERBOSE("  table alignment: %" PRIu32, mem_info->table_alignment);

    return true;

fail:
    return false;
}

static bool
load_needed_section(DynLinkLoaderCtx *ctx)
{
    DynLinkSectionNeeded *needed = &ctx->module->dynlink_data.needed;
    uint32 i;

    bh_assert(ctx);

    read_leb_uint32(ctx->ptr, ctx->ptr_end, needed->count);
    // TODO check_buf

    needed->entries =
        wasm_runtime_malloc(sizeof(*needed->entries) * needed->count);
    if (needed->entries == NULL) {
        set_error_buf(ctx, "out of memory");
        return false;
    }

    for (i = 0; i < needed->count; i++) {
        if (!(needed->entries[i] = load_string(ctx))) {
            goto fail;
        }
        LOG_VERBOSE("Load dylink needed entry success: %s", needed->entries[i]);
    }

    LOG_VERBOSE("Load dylink needed section success.")

    return true;

fail:
    wasm_runtime_free(needed->entries);

    return false;
}

static bool
load_export_info_section(DynLinkLoaderCtx *ctx)
{
    DynLinkSectionExportInfo *export_info =
        &ctx->module->dynlink_data.export_info;
    uint32 i;

    bh_assert(ctx);

    read_leb_uint32(ctx->ptr, ctx->ptr_end, export_info->count);
    // TODO check_buf
    export_info->entries =
        wasm_runtime_malloc(sizeof(*export_info->entries) * export_info->count);

    if (export_info->entries == NULL) {
        set_error_buf(ctx, "out of memory");
        return false;
    }

    for (i = 0; i < export_info->count; i++) {
        if (!(export_info->entries[i].name = load_string(ctx))) {
            goto fail;
        }
        read_leb_uint32(ctx->ptr, ctx->ptr_end, export_info->entries[i].flags);

        LOG_VERBOSE("Load dylink export entry success: %s %" PRIu32,
                    export_info->entries[i].name,
                    export_info->entries[i].flags);
    }

    LOG_VERBOSE("Load dylink export section success.")
fail:
    if (export_info->entries != NULL) {
        wasm_runtime_free(export_info->entries);
    }

    return false;
}

static bool
load_import_info_section(DynLinkLoaderCtx *ctx)
{
    DynLinkSectionImportInfo *import_info =
        &ctx->module->dynlink_data.import_info;

    bh_assert(ctx);

    if (!(import_info->module = load_string(ctx))) {
        goto fail;
    }

    if (!(import_info->field = load_string(ctx))) {
        goto fail;
    }

    read_leb_uint32(ctx->ptr, ctx->ptr_end, import_info->flags);

    LOG_VERBOSE("Load dylink import section success: %s %s %" PRIu32,
                import_info->module, import_info->field, import_info->flags);

    return true;

fail:
    return false;
}

static bool
dynlink_load_subsections(DynLinkLoaderCtx *ctx)
{
    while (ctx->ptr < ctx->ptr_end) {
        uint8 type = *ctx->ptr++;
        uint32 subsection_size;
        read_leb_uint32(ctx->ptr, ctx->ptr_end, subsection_size);
        CHECK_BUF(ctx->ptr, ctx->ptr_end, subsection_size);

        switch (type) {
            case WASM_DYLINK_MEM_INFO:
                if (!load_mem_info_section(ctx)) {
                    return false;
                }
                break;
            case WASM_DYLINK_NEEDED:
                if (!load_needed_section(ctx)) {
                    return false;
                }
                break;
            case WASM_DYLINK_EXPORT_INFO:
                if (!load_export_info_section(ctx)) {
                    return false;
                }
                break;
            case WASM_DYLINK_IMPORT_INFO:
                if (!load_import_info_section(ctx)) {
                    return false;
                }
                break;
            default:
                set_error_buf(ctx, "unknown subsection type");
                return false;
        }
    }

    return true;

fail:
    return false;
}

bool
dynlink_load_section(const uint8 *buf, const uint8 *buf_end, WASMModule *module,
                     bool is_load_from_file_buf, char *error_buf,
                     uint32 error_buf_size)
{
    const uint8 *p = buf, *p_end = buf_end;

    DynLinkLoaderCtx ctx = {
        .ptr = (uint8 *)p,
        .ptr_end = p_end,
        .module = module,
        .is_load_from_file_buf = is_load_from_file_buf,
        .error_buf = error_buf,
        .error_buf_size = error_buf_size,
    };

    if (p >= p_end) {
        set_error_buf(&ctx, "unexpected end");
        return false;
    }

    return dynlink_load_subsections(&ctx);
}