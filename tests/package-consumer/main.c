#include <cadpreview/cadpreview.h>

#include <stdio.h>
#include <string.h>

int main(void) {
    static const unsigned char data[] = {'c', 'a', 'd'};
    cp_preview* preview = NULL;
    struct cp_preview_info info;
    const void* png = NULL;
    size_t png_len = 0;
    int ok;

    if (cp_render_memory(data, sizeof data, NULL, &preview) != CP_OK) {
        fprintf(stderr, "render failed\n");
        return 1;
    }
    memset(&info, 0, sizeof info);
    info.struct_size = sizeof info;
    ok = cp_preview_info(preview, &info) == CP_OK &&
         cp_preview_png(preview, &png, &png_len) == CP_OK && png_len > 8 && info.width == 512;
    cp_preview_free(preview);
    if (!ok || strcmp(cp_version(), CP_VERSION_STRING) != 0) {
        fprintf(stderr, "installed package check failed\n");
        return 1;
    }
    printf("find_package(cadpreview) works: %s\n", cp_version());
    return 0;
}
