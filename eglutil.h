/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#ifndef EGLUTIL_H
#define EGLUTIL_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl32.h>
#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifdef __ANDROID__

#include <android/hardware_buffer.h>

#define LIBEGL_NAME "libEGL.so"

struct gbm_device;
struct gbm_bo;

#else /* __ANDROID__ */

#include <gbm.h>

#define LIBEGL_NAME "libEGL.so.1"

typedef struct AHardwareBuffer AHardwareBuffer;

#endif /* __ANDROID__ */

#define PRINTFLIKE(f, a) __attribute__((format(printf, f, a)))
#define NORETURN __attribute__((noreturn))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct egl_gl {
#define PFN_GL(proc, name) PFNGL##proc##PROC name;
#include "eglutil_entrypoints.inc"
};

struct egl_format {
    int drm_format;
    int drm_modifier_count;
    const EGLuint64KHR *drm_modifiers;
    const EGLBoolean *external_only;
};

struct egl {
    struct {
        void *handle;

#define PFN_EGL(proc, name) PFNEGL##proc##PROC name;
#include "eglutil_entrypoints.inc"
        struct egl_gl gl;

        const char *client_exts;
    };

    EGLDeviceEXT dev;
    EGLDisplay dpy;
    EGLint major;
    EGLint minor;

    const char *dpy_exts;
    bool KHR_no_config_context;
    bool EXT_image_dma_buf_import;
    bool EXT_image_dma_buf_import_modifiers;
    bool ANDROID_get_native_client_buffer;
    bool ANDROID_image_native_buffer;

    struct gbm_device *gbm;
    int gbm_fd;

    EGLConfig config;
    EGLSurface surf;

    EGLContext ctx;

    int format_count;
    struct egl_format **formats;

    const char *gl_exts;
};

struct egl_program {
    GLuint vs;
    GLuint fs;
    GLuint prog;
};

struct egl_image_info {
    int width;
    int height;
    int drm_format;
    /* DRM_FORMAT_MOD_INVALID, DRM_FORMAT_MOD_LINEAR or vendor ones */
    uint64_t drm_modifier;
};

struct egl_image_storage {
    AHardwareBuffer *ahb;
    struct gbm_bo *bo;
};

struct egl_image_map {
    int plane_count;
    void *planes[3];
    int row_strides[3];
    int pixel_strides[3];

    void *bo_xfer;
};

struct egl_image {
    struct egl_image_info info;
    struct egl_image_storage storage;
    EGLImage img;
};

static inline void
egl_logv(const char *format, va_list ap)
{
    printf("EGL: ");
    vprintf(format, ap);
    printf("\n");
}

static inline void NORETURN
egl_diev(const char *format, va_list ap)
{
    egl_logv(format, ap);
    abort();
}

static inline void PRINTFLIKE(1, 2) egl_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    egl_logv(format, ap);
    va_end(ap);
}

static inline void PRINTFLIKE(1, 2) NORETURN egl_die(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    egl_diev(format, ap);
    va_end(ap);
}

static inline void
egl_check(struct egl *egl, const char *where)
{
    const EGLint egl_err = egl->GetError();
    if (egl_err != EGL_SUCCESS)
        egl_die("%s: egl has error 0x%04x", where, egl_err);

    if (egl->ctx) {
        const GLenum gl_err = egl->gl.GetError();
        if (gl_err != GL_NO_ERROR)
            egl_die("%s: gl has error 0x%04x", where, gl_err);
    }
}

static inline int
egl_drm_format_to_cpp(int drm_format)
{
    switch (drm_format) {
    case DRM_FORMAT_ABGR16161616F:
        return 8;
    case DRM_FORMAT_P010:
        return 6;
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_GR1616:
        return 4;
    case DRM_FORMAT_BGR888:
    case DRM_FORMAT_NV12:
        return 3;
    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_GR88:
    case DRM_FORMAT_R16:
        return 2;
    case DRM_FORMAT_R8:
        return 1;
    default:
        egl_die("unsupported drm format 0x%x", drm_format);
    }
}

static inline int
egl_drm_format_to_plane_count(int drm_format)
{
    switch (drm_format) {
    case DRM_FORMAT_P010:
    case DRM_FORMAT_NV12:
        return 2;
    default:
        return 1;
    }
}

static inline int
egl_drm_format_to_plane_format(int drm_format, int plane)
{
    if (plane >= egl_drm_format_to_plane_count(drm_format))
        egl_die("bad plane");

    switch (drm_format) {
    case DRM_FORMAT_P010:
        return plane ? DRM_FORMAT_GR1616 : DRM_FORMAT_R16;
    case DRM_FORMAT_NV12:
        return plane ? DRM_FORMAT_GR88 : DRM_FORMAT_R8;
    default:
        return drm_format;
    }
}

#ifdef __ANDROID__

static inline void
egl_init_image_allocator(struct egl *egl)
{
    egl->gbm_fd = -1;
}

static inline void
egl_cleanup_image_allocator(struct egl *egl)
{
}

static inline enum AHardwareBuffer_Format
egl_drm_format_to_ahb_format(int drm_format)
{
    /* sanity check */
    egl_drm_format_to_cpp(drm_format);

    switch (drm_format) {
    case DRM_FORMAT_ABGR8888:
        return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    case DRM_FORMAT_XBGR8888:
        return AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
    case DRM_FORMAT_BGR888:
        return AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;
    case DRM_FORMAT_RGB565:
        return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
    case DRM_FORMAT_ABGR16161616F:
        return AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
    case DRM_FORMAT_ABGR2101010:
        return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
    case DRM_FORMAT_R8:
        return AHARDWAREBUFFER_FORMAT_BLOB;
#if __ANDROID_API__ >= 29
    case DRM_FORMAT_NV12:
        /* there is no guarantee gralloc would pick NV12.. */
        return AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420;
    case DRM_FORMAT_P010:
        return AHARDWAREBUFFER_FORMAT_YCbCr_P010;
#endif
    default:
        egl_die("unsupported drm format 0x%x", drm_format);
    }
}

static inline void
egl_alloc_image_storage(struct egl *egl, struct egl_image *img)
{
    const struct egl_image_info *info = &img->info;

    if (info->drm_modifier != DRM_FORMAT_MOD_INVALID)
        egl_die("drm modifier must be DRM_FORMAT_MOD_INVALID");

    const enum AHardwareBuffer_Format format = egl_drm_format_to_ahb_format(info->drm_format);
    const uint64_t usage =
        AHARDWAREBUFFER_USAGE_CPU_READ_RARELY | AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY |
        AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

    const AHardwareBuffer_Desc desc = {
        .width = info->width,
        .height = info->height,
        .layers = 1,
        .format = format,
        .usage = usage,
    };
    if (AHardwareBuffer_allocate(&desc, &img->storage.ahb))
        egl_die("failed to create ahb");
}

static inline void
egl_free_image_storage(struct egl *egl, struct egl_image *img)
{
    AHardwareBuffer_release(img->storage.ahb);
}

static inline void
egl_map_image_storage(struct egl *egl, const struct egl_image *img, struct egl_image_map *map)
{
    const struct egl_image_info *info = &img->info;
    const uint64_t usage =
        AHARDWAREBUFFER_USAGE_CPU_READ_RARELY | AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;
    const ARect rect = { .right = info->width, .bottom = info->height };

#if __ANDROID_API__ >= 29
    AHardwareBuffer_Planes planes;
    if (AHardwareBuffer_lockPlanes(img->storage.ahb, usage, -1, &rect, &planes))
        egl_die("failed to lock ahb");

    map->plane_count = planes.planeCount;
    for (int i = 0; i < map->plane_count; i++) {
        map->planes[i] = planes.planes[i].data;
        map->row_strides[i] = planes.planes[i].rowStride;
        map->pixel_strides[i] = planes.planes[i].pixelStride;
    }
#else
    if (egl_drm_format_to_plane_count(info->drm_format) > 1)
        egl_die("no AHardwareBuffer_lockPlanes support");

    void *ptr;
    if (AHardwareBuffer_lock(img->storage.ahb, usage, -1, &rect, &ptr))
        egl_die("failed to lock ahb");

    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(img->storage.ahb, &desc);

    const int cpp = egl_drm_format_to_cpp(info->drm_format);
    map->plane_count = 1;
    map->planes[0] = ptr;
    map->row_strides[0] = desc.stride * cpp;
    map->pixel_strides[0] = cpp;
#endif

    map->bo_xfer = NULL;
}

static inline void
egl_unmap_image_storage(struct egl *egl, struct egl_image *img, struct egl_image_map *map)
{
    AHardwareBuffer_unlock(img->storage.ahb, NULL);
}

static inline void
egl_wrap_image_storage(struct egl *egl, struct egl_image *img)
{
    if (!egl->ANDROID_get_native_client_buffer || !egl->ANDROID_image_native_buffer)
        egl_die("no ahb import support");

    EGLClientBuffer buf = egl->GetNativeClientBufferANDROID(img->storage.ahb);
    if (!buf)
        egl_die("failed to get client buffer from ahb");

    const EGLAttrib img_attrs[] = {
        EGL_IMAGE_PRESERVED,
        EGL_TRUE,
        EGL_NONE,
    };

    img->img =
        egl->CreateImage(egl->dpy, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, buf, img_attrs);
    if (img->img == EGL_NO_IMAGE)
        egl_die("failed to create img");
}

#else /* __ANDROID__ */

static inline void
egl_init_image_allocator(struct egl *egl)
{
    if (egl->dev == EGL_NO_DEVICE_EXT)
        egl_die("gbm requires EGLDeviceEXT");

    const char *node = egl->QueryDeviceStringEXT(egl->dev, EGL_DRM_RENDER_NODE_FILE_EXT);
    egl->gbm_fd = open(node, O_RDWR | O_CLOEXEC);
    if (egl->gbm_fd < 0)
        egl_die("failed to open %s", node);

    egl->gbm = gbm_create_device(egl->gbm_fd);
    if (!egl->gbm)
        egl_die("failed to create gbm device");
}

static inline void
egl_cleanup_image_allocator(struct egl *egl)
{
    gbm_device_destroy(egl->gbm);
    close(egl->gbm_fd);
}

static inline void
egl_alloc_image_storage(struct egl *egl, struct egl_image *img)
{
    const struct egl_image_info *info = &img->info;
    /* This is only to make minigbm happy.
     *
     * mesa gbm does not support planar.  When the format is planar, it must
     * be minigbm and GBM_BO_USE_PROTECTED (bit 5) means GBM_BO_USE_TEXTURING
     * to minigbm instead.
     */
    const uint32_t minigbm_flags = egl_drm_format_to_plane_count(info->drm_format) > 1
                                       ? GBM_BO_USE_PROTECTED
                                       : GBM_BO_USE_RENDERING;

    struct gbm_bo *bo;
    if (info->drm_modifier != DRM_FORMAT_MOD_INVALID) {
        bo = gbm_bo_create_with_modifiers2(egl->gbm, info->width, info->height, info->drm_format,
                                           &info->drm_modifier, 1, minigbm_flags);
    } else {
        bo = gbm_bo_create(egl->gbm, info->width, info->height, info->drm_format, minigbm_flags);
    }
    if (!bo)
        egl_die("failed to create gbm bo");

    const int plane_count = gbm_bo_get_plane_count(bo);
    if (plane_count > 1) {
        /* make sure all planes have the same handle (for mapping) */
        const union gbm_bo_handle handle = gbm_bo_get_handle_for_plane(bo, 0);
        for (int i = 1; i < plane_count; i++) {
            const union gbm_bo_handle h = gbm_bo_get_handle_for_plane(bo, i);
            if (memcmp(&handle, &h, sizeof(h)))
                egl_die("bo planes have different handles");
        }
    }

    img->storage.bo = bo;
}

static inline void
egl_free_image_storage(struct egl *egl, struct egl_image *img)
{
    gbm_bo_destroy(img->storage.bo);
}

static inline void
egl_map_image_storage(struct egl *egl, struct egl_image *img, struct egl_image_map *map)
{
    const struct egl_image_info *info = &img->info;
    struct gbm_bo *bo = img->storage.bo;

    uint32_t stride;
    void *xfer = NULL;
    void *ptr = gbm_bo_map(bo, 0, 0, info->width, info->height, GBM_BO_TRANSFER_READ_WRITE,
                           &stride, &xfer);
    if (!ptr)
        egl_die("failed to map bo");

    map->plane_count = egl_drm_format_to_plane_count(info->drm_format);
    if (map->plane_count > 1) {
        if (map->plane_count > gbm_bo_get_plane_count(bo))
            egl_die("unexpected bo plane count");

        for (int i = 0; i < map->plane_count; i++) {
            map->planes[i] = ptr + gbm_bo_get_offset(bo, i);
            map->row_strides[i] = gbm_bo_get_stride_for_plane(bo, i);
            map->pixel_strides[i] =
                egl_drm_format_to_cpp(egl_drm_format_to_plane_format(info->drm_format, i));
        }

        /* Y and UV, and 4:2:0 subsampling */
        if (map->plane_count == 2) {
            map->row_strides[1] /= 2;
            map->pixel_strides[1] /= 2;

            map->plane_count = 3;
            map->planes[2] = map->planes[1] + map->pixel_strides[1];
            map->row_strides[2] = map->row_strides[1];
            map->pixel_strides[2] = map->pixel_strides[1];
        }
    } else {
        map->planes[0] = ptr;
        map->row_strides[0] = stride;
        map->pixel_strides[0] = egl_drm_format_to_cpp(info->drm_format);
    }

    map->bo_xfer = xfer;
}

static inline void
egl_unmap_image_storage(struct egl *egl, struct egl_image *img, struct egl_image_map *map)
{
    struct egl_image_storage *storage = &img->storage;

    gbm_bo_unmap(storage->bo, map->bo_xfer);
}

static inline int
egl_image_to_dma_buf_attrs(const struct egl_image *img, EGLAttrib *attrs, int count)
{
    const struct egl_image_info *info = &img->info;
    struct gbm_bo *bo = img->storage.bo;

    assert(count >= 64);
    int c = 0;
    attrs[c++] = EGL_IMAGE_PRESERVED;
    attrs[c++] = EGL_TRUE;
    attrs[c++] = EGL_WIDTH;
    attrs[c++] = info->width;
    attrs[c++] = EGL_HEIGHT;
    attrs[c++] = info->height;
    attrs[c++] = EGL_LINUX_DRM_FOURCC_EXT;
    attrs[c++] = info->drm_format;

    const int fd = gbm_bo_get_fd_for_plane(bo, 0);
    if (fd < 0)
        egl_die("failed to export gbm bo");
    const int plane_count = gbm_bo_get_plane_count(bo);
    for (int i = 0; i < plane_count; i++) {
        const int offset = gbm_bo_get_offset(bo, i);
        const int stride = gbm_bo_get_stride_for_plane(bo, i);

        static_assert(GBM_MAX_PLANES <= 4, "");
        if (i < 3) {
            attrs[c++] = EGL_DMA_BUF_PLANE0_FD_EXT + 3 * i;
            attrs[c++] = fd;
            attrs[c++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT + 3 * i;
            attrs[c++] = offset;
            attrs[c++] = EGL_DMA_BUF_PLANE0_PITCH_EXT + 3 * i;
            attrs[c++] = stride;
        } else {
            attrs[c++] = EGL_DMA_BUF_PLANE3_FD_EXT;
            attrs[c++] = fd;
            attrs[c++] = EGL_DMA_BUF_PLANE3_OFFSET_EXT;
            attrs[c++] = offset;
            attrs[c++] = EGL_DMA_BUF_PLANE3_PITCH_EXT;
            attrs[c++] = stride;
        }
        attrs[c++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT + 2 * i;
        attrs[c++] = (EGLAttrib)info->drm_modifier;
        attrs[c++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT + 2 * i;
        attrs[c++] = (EGLAttrib)(info->drm_modifier >> 32);
    }

    attrs[c++] = EGL_NONE;
    assert(c <= count);

    return fd;
}

static inline void
egl_wrap_image_storage(struct egl *egl, struct egl_image *img)
{
    if (!egl->EXT_image_dma_buf_import || !egl->EXT_image_dma_buf_import_modifiers)
        egl_die("no dma-buf import support");

    EGLAttrib img_attrs[64];
    const int fd = egl_image_to_dma_buf_attrs(img, img_attrs, ARRAY_SIZE(img_attrs));

    img->img = egl->CreateImage(egl->dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, img_attrs);
    if (img->img == EGL_NO_IMAGE)
        egl_die("failed to create img");

    close(fd);
}

#endif /* __ANDROID__ */

static inline void
egl_init_library_dispatch(struct egl *egl)
{
    /* we assume EGL 1.5, which includes EGL_EXT_client_extensions and
     * EGL_KHR_client_get_all_proc_addresses
     */
#define PFN_EGL_EXT(proc, name) egl->name = (PFNEGL##proc##PROC)egl->GetProcAddress("egl" #name);
#define PFN_GL_EXT(proc, name) egl->gl.name = (PFNGL##proc##PROC)egl->GetProcAddress("gl" #name);
#define PFN_EGL(proc, name)                                                                      \
    PFN_EGL_EXT(proc, name)                                                                      \
    if (!egl->name)                                                                              \
        egl_die("no egl" #name);
#define PFN_GL(proc, name)                                                                       \
    PFN_GL_EXT(proc, name)                                                                       \
    if (!egl->gl.name)                                                                           \
        egl_die("no gl" #name);
#include "eglutil_entrypoints.inc"
}

static inline void
egl_init_library(struct egl *egl)
{
    egl->handle = dlopen(LIBEGL_NAME, RTLD_LOCAL | RTLD_LAZY);
    if (!egl->handle)
        egl_die("failed to load %s: %s", LIBEGL_NAME, dlerror());

    const char gipa_name[] = "eglGetProcAddress";
    egl->GetProcAddress = dlsym(egl->handle, gipa_name);
    if (!egl->GetProcAddress)
        egl_die("failed to find %s: %s", gipa_name, dlerror());

    egl_init_library_dispatch(egl);

    egl->client_exts = egl->QueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (!egl->client_exts) {
#ifdef __ANDROID__
        egl_log("no client extension");
        egl->client_exts = "";
        egl->GetError();
#else
        egl_die("no client extension");
#endif
    }
}

static inline void
egl_init_display_extensions(struct egl *egl)
{
    egl->dpy_exts = egl->QueryString(egl->dpy, EGL_EXTENSIONS);

    egl->KHR_no_config_context = strstr(egl->dpy_exts, "EGL_KHR_no_config_context");
    egl->EXT_image_dma_buf_import = strstr(egl->dpy_exts, "EGL_EXT_image_dma_buf_import");
    egl->EXT_image_dma_buf_import_modifiers =
        strstr(egl->dpy_exts, "EGL_EXT_image_dma_buf_import_modifiers");
    egl->ANDROID_get_native_client_buffer =
        strstr(egl->dpy_exts, "EGL_ANDROID_get_native_client_buffer");
    egl->ANDROID_image_native_buffer = strstr(egl->dpy_exts, "EGL_ANDROID_image_native_buffer");
}

static inline void
egl_init_display(struct egl *egl)
{
    const bool EXT_device_enumeration = strstr(egl->client_exts, "EGL_EXT_device_enumeration");
    const bool EXT_device_query = strstr(egl->client_exts, "EGL_EXT_device_query");
    const bool EXT_platform_device = strstr(egl->client_exts, "EGL_EXT_platform_device");
    const bool KHR_platform_android = strstr(egl->client_exts, "EGL_KHR_platform_android");

    if (EXT_device_enumeration && EXT_device_query && EXT_platform_device) {
        egl_log("using platform device");

        EGLDeviceEXT devs[16];
        EGLint count;
        if (!egl->QueryDevicesEXT(ARRAY_SIZE(devs), devs, &count))
            egl_die("failed to query devices");

        egl->dev = EGL_NO_DEVICE_EXT;
        for (int i = 0; i < count; i++) {
            const char *exts = egl->QueryDeviceStringEXT(devs[i], EGL_EXTENSIONS);
            /* EGL_EXT_device_drm_render_node and not EGL_MESA_device_software */
            if (strstr(exts, "EGL_EXT_device_drm_render_node") && !strstr(exts, "software")) {
                egl->dev = devs[i];
                break;
            }
        }
        if (egl->dev == EGL_NO_DEVICE_EXT)
            egl_die("failed to find a hw rendernode device");

        egl->dpy = egl->GetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, egl->dev, NULL);
    } else if (KHR_platform_android) {
        egl_log("using platform android");

        egl->dev = EGL_NO_DEVICE_EXT;
        egl->dpy = egl->GetPlatformDisplay(EGL_PLATFORM_ANDROID_KHR, EGL_DEFAULT_DISPLAY, NULL);
    } else {
        egl_log("using EGL_DEFAULT_DISPLAY");
        egl->dev = EGL_NO_DEVICE_EXT;
        egl->dpy = egl->GetDisplay(EGL_DEFAULT_DISPLAY);
    }

    if (egl->dpy == EGL_NO_DISPLAY)
        egl_die("failed to get platform display");

    if (!egl->Initialize(egl->dpy, &egl->major, &egl->minor))
        egl_die("failed to initialize display");

    egl_init_display_extensions(egl);

    if (egl->major != 1 || egl->minor < 5) {
#ifdef __ANDROID__
        egl_log("fixing up for EGL %d.%d", egl->major, egl->minor);
        if (!strstr(egl->dpy_exts, "EGL_KHR_image_base"))
            egl_die("no EGL_KHR_image_base");
        egl->CreateImage = (PFNEGLCREATEIMAGEPROC)egl->GetProcAddress("eglCreateImageKHR");
        egl->DestroyImage = (PFNEGLDESTROYIMAGEPROC)egl->GetProcAddress("eglDestroyImageKHR");
#else
        egl_die("EGL 1.5 is required");
#endif
    }
}

static inline void
egl_init_config_and_surface(struct egl *egl, EGLint pbuffer_width, EGLint pbuffer_height)
{
    const bool with_pbuffer = pbuffer_width && pbuffer_height;
    if (egl->KHR_no_config_context && !with_pbuffer) {
        egl_log("using EGL_NO_CONFIG_KHR");
        egl->config = EGL_NO_CONFIG_KHR;
        return;
    }

    const EGLint config_attrs[] = {
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,
        with_pbuffer ? EGL_PBUFFER_BIT : 0,
        EGL_NONE,
    };

    EGLint count;
    if (!egl->ChooseConfig(egl->dpy, config_attrs, &egl->config, 1, &count) || !count)
        egl_die("failed to choose a config");

    if (!with_pbuffer) {
        egl_log("using EGL_NO_SURFACE");
        egl->surf = EGL_NO_SURFACE;
        return;
    }

    const EGLint surf_attrs[] = {
        EGL_WIDTH, pbuffer_width, EGL_HEIGHT, pbuffer_height, EGL_NONE,
    };

    egl->surf = egl->CreatePbufferSurface(egl->dpy, egl->config, surf_attrs);
    if (egl->surf == EGL_NO_SURFACE)
        egl_die("failed to create pbuffer surface");
}

static inline void
egl_init_context(struct egl *egl)
{
    if (egl->QueryAPI() != EGL_OPENGL_ES_API)
        egl_die("current api is not GLES");

    const EGLint ctx_attrs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE,
    };

    EGLContext ctx = egl->CreateContext(egl->dpy, egl->config, EGL_NO_CONTEXT, ctx_attrs);
    if (ctx == EGL_NO_CONTEXT)
        egl_die("failed to create a context");

    if (!egl->MakeCurrent(egl->dpy, egl->surf, egl->surf, ctx))
        egl_die("failed to make context current");

    egl->ctx = ctx;
}

static inline void
egl_init_formats(struct egl *egl)
{
    if (!egl->EXT_image_dma_buf_import_modifiers)
        return;

    EGLint fmt_count;
    if (!egl->QueryDmaBufFormatsEXT(egl->dpy, 0, NULL, &fmt_count))
        egl_die("failed to get dma-buf format count");

    struct egl_format **fmts = malloc(sizeof(*fmts) * fmt_count);
    EGLint *drm_fmts = malloc(sizeof(*drm_fmts) * fmt_count);
    if (!fmts || !drm_fmts)
        egl_die("failed to alloc fmts");

    if (!egl->QueryDmaBufFormatsEXT(egl->dpy, fmt_count, drm_fmts, &fmt_count))
        egl_die("failed to get dma-buf formats");

    for (int i = 0; i < fmt_count; i++) {
        const EGLint drm_fmt = drm_fmts[i];

        EGLint mod_count;
        if (!egl->QueryDmaBufModifiersEXT(egl->dpy, drm_fmt, 0, NULL, NULL, &mod_count))
            egl_die("failed to get dma-buf modifier count");

        struct egl_format *fmt = malloc(sizeof(*fmt) + sizeof(fmt->drm_modifiers) * mod_count +
                                        sizeof(fmt->external_only) * mod_count);
        if (!fmt)
            egl_die("failed to alloc fmt");
        EGLuint64KHR *drm_modifiers = (EGLuint64KHR *)(fmt + 1);
        EGLBoolean *external_only = (EGLBoolean *)(drm_modifiers + mod_count);

        if (!egl->QueryDmaBufModifiersEXT(egl->dpy, drm_fmt, mod_count, drm_modifiers,
                                          external_only, &mod_count))
            egl_die("failed to get dma-buf modifiers");

        fmt->drm_format = drm_fmt;
        fmt->drm_modifier_count = mod_count;
        fmt->drm_modifiers = drm_modifiers;
        fmt->external_only = external_only;
        fmts[i] = fmt;
    }

    free(drm_fmts);

    egl->format_count = fmt_count;
    egl->formats = fmts;
}

static inline void
egl_init_gl(struct egl *egl)
{
    egl->gl_exts = (const char *)egl->gl.GetString(GL_EXTENSIONS);
    if (!egl->gl_exts)
        egl_die("no GLES extensions");
}

static inline void
egl_init(struct egl *egl, EGLint pbuffer_width, EGLint pbuffer_height)
{
    memset(egl, 0, sizeof(*egl));

    egl_init_library(egl);
    egl_check(egl, "init library");

    egl_init_display(egl);
    egl_check(egl, "init display");

    egl_init_image_allocator(egl);
    egl_check(egl, "init image allocator");

    egl_init_config_and_surface(egl, pbuffer_width, pbuffer_height);
    egl_check(egl, "init config and surface");

    egl_init_context(egl);
    egl_check(egl, "init context");

    egl_init_formats(egl);
    egl_check(egl, "init formats");

    egl_init_gl(egl);
    egl_check(egl, "init gl");
}

static inline void
egl_cleanup(struct egl *egl)
{
    egl_check(egl, "cleanup");

    if (egl->format_count) {
        for (int i = 0; i < egl->format_count; i++)
            free(egl->formats[i]);
        free(egl->formats);
    }

    egl->MakeCurrent(egl->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    egl->DestroyContext(egl->dpy, egl->ctx);
    egl->DestroySurface(egl->dpy, egl->surf);

    egl_cleanup_image_allocator(egl);

    egl->Terminate(egl->dpy);
    egl->ReleaseThread();

    dlclose(egl->handle);
}

static inline void
egl_dump_formats(struct egl *egl)
{
    for (int i = 0; i < egl->format_count; i++) {
        const struct egl_format *fmt = egl->formats[i];

        egl_log("format %d: %c%c%c%c (0x%08x)", i, (fmt->drm_format >> 0) & 0xff,
                (fmt->drm_format >> 8) & 0xff, (fmt->drm_format >> 16) & 0xff,
                (fmt->drm_format >> 24) & 0xff, fmt->drm_format);
        for (int j = 0; j < fmt->drm_modifier_count; j++) {
            egl_log("  modifier 0x%016" PRIx64 " external only %d", fmt->drm_modifiers[j],
                    fmt->external_only[j]);
        }
    }
}

static inline const void *
egl_parse_ppm(const void *ppm_data, size_t ppm_size, int *width, int *height)
{
    if (sscanf(ppm_data, "P6 %d %d 255\n", width, height) != 2)
        egl_die("invalid ppm header");

    const size_t img_size = *width * *height * 3;
    if (img_size >= ppm_size)
        egl_die("bad ppm dimension %dx%d", *width, *height);

    const size_t hdr_size = ppm_size - img_size;
    if (!isspace(((const char *)ppm_data)[hdr_size - 1]))
        egl_die("no space at the end of ppm header");

    return ppm_data + hdr_size;
}

static inline void
egl_write_ppm(const char *filename, const void *data, int width, int height)
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
        egl_die("failed to open %s", filename);

    fprintf(fp, "P6 %d %d 255\n", width, height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const void *pixel = data + ((width * y) + x) * 4;
            if (fwrite(pixel, 3, 1, fp) != 1)
                egl_die("failed to write pixel (%d, %x)", x, y);
        }
    }

    fclose(fp);
}

static inline void
egl_dump_image(struct egl *egl, int width, int height, const char *filename)
{
    const GLenum format = GL_RGBA;
    const GLenum type = GL_UNSIGNED_BYTE;
    const GLsizei size = width * height * 4;

    char *data = malloc(size);
    if (!data)
        egl_die("failed to alloc readback buf");

    egl->gl.ReadnPixels(0, 0, width, height, format, type, size, data);
    egl_check(egl, "dump");

    egl_write_ppm(filename, data, width, height);

    free(data);
}

static inline GLuint
egl_compile_shader(struct egl *egl, GLenum type, const char *glsl)
{
    struct egl_gl *gl = &egl->gl;

    GLuint sh = gl->CreateShader(type);
    gl->ShaderSource(sh, 1, &glsl, NULL);
    gl->CompileShader(sh);

    GLint val;
    gl->GetShaderiv(sh, GL_COMPILE_STATUS, &val);
    if (val != GL_TRUE) {
        char info_log[1024];
        gl->GetShaderInfoLog(sh, sizeof(info_log), NULL, info_log);
        egl_die("failed to compile shader: %s", info_log);
    }

    return sh;
}

static inline GLuint
egl_link_program(struct egl *egl, const GLuint *shaders, int count)
{
    struct egl_gl *gl = &egl->gl;

    GLuint prog = gl->CreateProgram();
    for (int i = 0; i < count; i++)
        gl->AttachShader(prog, shaders[i]);
    gl->LinkProgram(prog);

    GLint val;
    gl->GetProgramiv(prog, GL_LINK_STATUS, &val);
    if (val != GL_TRUE) {
        char info_log[1024];
        gl->GetProgramInfoLog(prog, sizeof(info_log), NULL, info_log);
        egl_die("failed to link program: %s", info_log);
    }

    return prog;
}

static inline struct egl_program *
egl_create_program(struct egl *egl, const char *vs_glsl, const char *fs_glsl)
{
    struct egl_program *prog = calloc(1, sizeof(*prog));
    if (!prog)
        egl_die("failed to alloc prog");

    prog->vs = egl_compile_shader(egl, GL_VERTEX_SHADER, vs_glsl);
    prog->fs = egl_compile_shader(egl, GL_FRAGMENT_SHADER, fs_glsl);

    const GLuint shaders[] = { prog->vs, prog->fs };
    prog->prog = egl_link_program(egl, shaders, ARRAY_SIZE(shaders));

    return prog;
}

static inline void
egl_destroy_program(struct egl *egl, struct egl_program *prog)
{
    struct egl_gl *gl = &egl->gl;

    gl->DeleteProgram(prog->prog);
    gl->DeleteShader(prog->vs);
    gl->DeleteShader(prog->fs);

    free(prog);
}

static inline struct egl_image *
egl_create_image(struct egl *egl, const struct egl_image_info *info)
{
    struct egl_image *img = calloc(1, sizeof(*img));
    if (!img)
        egl_die("failed to alloc img");

    img->info = *info;
    egl_alloc_image_storage(egl, img);
    egl_wrap_image_storage(egl, img);

    return img;
}

static inline void
egl_rgb_to_yuv(const uint8_t *rgb, uint8_t *yuv)
{
    const int tmp[3] = {
        ((66 * (rgb)[0] + 129 * (rgb)[1] + 25 * (rgb)[2] + 128) >> 8) + 16,
        ((-38 * (rgb)[0] - 74 * (rgb)[1] + 112 * (rgb)[2] + 128) >> 8) + 128,
        ((112 * (rgb)[0] - 94 * (rgb)[1] - 18 * (rgb)[2] + 128) >> 8) + 128,
    };

    for (int i = 0; i < 3; i++) {
        if (tmp[i] > 255)
            yuv[i] = 255;
        else if (tmp[i] < 0)
            yuv[i] = 0;
        else
            yuv[i] = tmp[i];
    }
}

static inline struct egl_image *
egl_create_image_from_ppm(struct egl *egl, const void *ppm_data, size_t ppm_size, bool planar)
{
    int width;
    int height;
    ppm_data = egl_parse_ppm(ppm_data, ppm_size, &width, &height);

    const struct egl_image_info img_info = {
        .width = width,
        .height = height,
        .drm_format = planar ? DRM_FORMAT_NV12 : DRM_FORMAT_ABGR8888,
        .drm_modifier = DRM_FORMAT_MOD_INVALID,
    };
    struct egl_image *img = egl_create_image(egl, &img_info);

    struct egl_image_map map;
    egl_map_image_storage(egl, img, &map);

    if (map.plane_count != (planar ? 3 : 1))
        egl_die("unexpected plane count");

    if (planar) {
        for (int y = 0; y < height; y++) {
            uint8_t *rows[3];
            for (int i = 0; i < map.plane_count; i++)
                rows[i] = map.planes[i] + map.row_strides[i] * y;

            for (int x = 0; x < width; x++) {
                uint8_t yuv[3];
                egl_rgb_to_yuv(ppm_data, yuv);
                ppm_data += 3;

                const int write_count = (x | y) & 1 ? 1 : 3;
                for (int i = 0; i < write_count; i++)
                    rows[i][map.pixel_strides[i] * x] = yuv[i];
            }
        }
    } else {
        for (int y = 0; y < height; y++) {
            uint8_t *dst = map.planes[0] + map.row_strides[0] * y;
            for (int x = 0; x < width; x++) {
                memcpy(dst, ppm_data, 3);
                dst[3] = 0xff;

                ppm_data += 3;
                dst += map.pixel_strides[0];
            }
        }
    }

    egl_unmap_image_storage(egl, img, &map);

    return img;
}

static inline void
egl_destroy_image(struct egl *egl, struct egl_image *img)
{
    egl->DestroyImage(egl->dpy, img->img);
    egl_free_image_storage(egl, img);
    free(img);
}

#endif /* EGLUTIL_H */
