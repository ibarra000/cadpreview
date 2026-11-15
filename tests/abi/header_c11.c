#include <cadpreview/cadpreview.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
_Static_assert(sizeof(cp_options) == 32, "cp_options v1 layout changed");
_Static_assert(offsetof(cp_options, log) == 16, "cp_options v1 layout changed");
_Static_assert(sizeof(struct cp_preview_info) == 72, "cp_preview_info v1 layout changed");
_Static_assert(offsetof(struct cp_preview_info, render_time_us) == 32,
               "cp_preview_info layout changed");
_Static_assert(offsetof(struct cp_preview_info, format_name) == 40,
               "cp_preview_info layout changed");
#endif
_Static_assert(sizeof(cp_format_info) == 104, "cp_format_info v1 layout changed");
_Static_assert(sizeof(cp_status) == 4 && sizeof(cp_reason) == 4, "enum types must be 32-bit");

static int check_placeholder(void) {
    static const unsigned char data[] = {1, 2, 3};
    cp_options opts;
    cp_preview* preview = NULL;
    struct cp_preview_info info;
    int ok;

    memset(&opts, 0, sizeof opts);
    opts.struct_size = sizeof opts;
    opts.size = 64;
    opts.tier = CP_TIER_AUTO;
    if (cp_render_memory(data, sizeof data, &opts, &preview) != CP_OK || preview == NULL) {
        return 0;
    }
    memset(&info, 0, sizeof info);
    info.struct_size = sizeof info;
    ok = cp_preview_info(preview, &info) == CP_OK && info.ok == 0 &&
         info.tier == CP_TIER_PLACEHOLDER && info.width == 64 &&
         strcmp(cp_reason_name(info.reason_code), "unknown_format") == 0;
    cp_preview_free(preview);
    return ok;
}

int main(void) {
    if (strcmp(cp_version(), CP_VERSION_STRING) != 0) {
        fprintf(stderr, "version mismatch: %s vs %s\n", cp_version(), CP_VERSION_STRING);
        return 1;
    }
    if (!check_placeholder()) {
        fprintf(stderr, "placeholder check failed\n");
        return 1;
    }
    printf("cadpreview %s C API ok\n", cp_version());
    return 0;
}
