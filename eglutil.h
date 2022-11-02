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

    const char *gl_exts;
};

struct egl_program {
    GLuint vs;
    GLuint fs;
    GLuint prog;
};

struct egl_bo_info {
    int width;
    int height;
    int drm_format;
    /* DRM_FORMAT_MOD_INVALID, DRM_FORMAT_MOD_LINEAR or vendor ones */
    uint64_t drm_modifier;
};

struct egl_bo {
    struct egl_bo_info info;
    int stride;

    AHardwareBuffer *ahb;
    struct gbm_bo *bo;
    void *bo_xfer;
};

struct egl_image {
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

#ifdef __ANDROID__

static inline void
egl_init_bo_allocator(struct egl *egl)
{
    egl->gbm_fd = -1;
}

static inline void
egl_cleanup_bo_allocator(struct egl *egl)
{
}

static inline void
egl_alloc_bo_storage(struct egl *egl, struct egl_bo *bo)
{
    if (bo->info.drm_format != DRM_FORMAT_ABGR8888)
        egl_die("drm format must be ABGR8888");
    if (bo->info.drm_modifier != DRM_FORMAT_MOD_INVALID)
        egl_die("drm modifier must be DRM_FORMAT_MOD_INVALID");

    const uint32_t format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    const uint64_t usage =
        AHARDWAREBUFFER_USAGE_CPU_READ_RARELY | AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY |
        AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

    AHardwareBuffer_Desc desc = {
        .width = bo->info.width,
        .height = bo->info.height,
        .layers = 1,
        .format = format,
        .usage = usage,
    };
    if (AHardwareBuffer_allocate(&desc, &bo->ahb))
        egl_die("failed to create ahb");

    AHardwareBuffer_describe(bo->ahb, &desc);
    bo->stride = desc.stride;
}

static inline void
egl_free_bo_storage(struct egl *egl, struct egl_bo *bo)
{
    AHardwareBuffer_release(bo->ahb);
}

static inline void
egl_map_bo_storage(struct egl *egl, struct egl_bo *bo)
{
    const uint64_t usage =
        AHARDWAREBUFFER_USAGE_CPU_READ_RARELY | AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;
    const ARect rect = {.right = bo->info.width, .bottom = bo->info.heigh } void * map;
    if (AHardwareBuffer_lock(bo->ahb, usage, -1, 0, bo->info.width, bo->info.height, &rect, &map))
        egl_die("failed to lock ahb");

    return map;
}

static inline void
egl_unmap_bo_storage(struct egl *egl, struct egl_bo *bo)
{
    AHardwareBuffer_unlock(bo->ahb, NULL);
}

static inline EGLImage
egl_wrap_bo_storage(struct egl *egl, const struct egl_bo *bo)
{
    if (!egl->ANDROID_get_native_client_buffer || !egl->ANDROID_image_native_buffer)
        egl_die("no ahb import support");

    EGLClientBuffer buf = egl->GetNativeClientBufferANDROID(bo->ahb);
    if (!buf)
        egl_die("failed to get client buffer from ahb");

    const EGLAttrib img_attrs[] = {
        EGL_IMAGE_PRESERVED,
        EGL_TRUE,
        EGL_NONE,
    };

    return egl->CreateImage(egl->dpy, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, buf, img_attrs);
}

#else /* __ANDROID__ */

static inline void
egl_init_bo_allocator(struct egl *egl)
{
    if (egl->dev == EGL_NO_DEVICE_EXT)
        egl_die("no device");

    const char *node = egl->QueryDeviceStringEXT(egl->dev, EGL_DRM_RENDER_NODE_FILE_EXT);
    egl->gbm_fd = open(node, O_RDWR | O_CLOEXEC);
    if (egl->gbm_fd < 0)
        egl_die("failed to open %s", node);

    egl->gbm = gbm_create_device(egl->gbm_fd);
    if (!egl->gbm)
        egl_die("failed to create gbm device");
}

static inline void
egl_cleanup_bo_allocator(struct egl *egl)
{
    gbm_device_destroy(egl->gbm);
    close(egl->gbm_fd);
}

static inline void
egl_alloc_bo_storage(struct egl *egl, struct egl_bo *bo)
{
    if (bo->info.drm_modifier != DRM_FORMAT_MOD_INVALID) {
        bo->bo = gbm_bo_create_with_modifiers2(egl->gbm, bo->info.width, bo->info.height,
                                               bo->info.drm_format, &bo->info.drm_modifier, 1, 0);
    } else {
        bo->bo = gbm_bo_create(egl->gbm, bo->info.width, bo->info.height, bo->info.drm_format, 0);
    }
    if (!bo->bo)
        egl_die("failed to create gbm bo");

    bo->stride = gbm_bo_get_stride_for_plane(bo->bo, 0);
}

static inline void
egl_free_bo_storage(struct egl *egl, struct egl_bo *bo)
{
    gbm_bo_destroy(bo->bo);
}

static inline void *
egl_map_bo_storage(struct egl *egl, struct egl_bo *bo)
{
    if (bo->bo_xfer)
        egl_die("recursive map");

    uint32_t stride;
    void *map = gbm_bo_map(bo->bo, 0, 0, bo->info.width, bo->info.height,
                           GBM_BO_TRANSFER_READ_WRITE, &stride, &bo->bo_xfer);
    if (!map)
        egl_die("failed to map bo");
    if (stride != (uint32_t)bo->stride)
        egl_die("unexpected map stride %d", stride);

    return map;
}

static inline void
egl_unmap_bo_storage(struct egl *egl, struct egl_bo *bo)
{
    gbm_bo_unmap(bo->bo, bo->bo_xfer);
}

static inline EGLImage
egl_wrap_bo_storage(struct egl *egl, const struct egl_bo *bo)
{
    if (!egl->EXT_image_dma_buf_import || !egl->EXT_image_dma_buf_import_modifiers)
        egl_die("no dma-buf import support");

    const int fd = gbm_bo_get_fd_for_plane(bo->bo, 0);
    if (fd < 0)
        egl_die("failed to export gbm bo");

    const EGLAttrib img_attrs[] = {
        EGL_IMAGE_PRESERVED,
        EGL_TRUE,
        EGL_DMA_BUF_PLANE0_FD_EXT,
        fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT,
        0,
        EGL_WIDTH,
        bo->info.width,
        EGL_HEIGHT,
        bo->info.height,
        EGL_DMA_BUF_PLANE0_PITCH_EXT,
        bo->stride,
        EGL_LINUX_DRM_FOURCC_EXT,
        bo->info.drm_format,
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
        (EGLint)bo->info.drm_modifier,
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
        (EGLint)(bo->info.drm_modifier >> 32),
        EGL_NONE,
    };

    EGLImage img =
        egl->CreateImage(egl->dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, img_attrs);
    close(fd);

    return img;
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
    if (!egl->client_exts)
        egl_die("no client extension");
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
        egl_die("no supported platform extension");
    }

    if (egl->dpy == EGL_NO_DISPLAY)
        egl_die("failed to get platform display");

    if (!egl->Initialize(egl->dpy, &egl->major, &egl->minor))
        egl_die("failed to initialize display");

    if (egl->major != 1 || egl->minor < 5)
        egl_die("EGL 1.5 is required");

    egl_init_display_extensions(egl);
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
egl_init_gl(struct egl *egl)
{
    egl->gl_exts = egl->gl.GetString(GL_EXTENSIONS);
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

    egl_init_bo_allocator(egl);
    egl_check(egl, "init bo allocator");

    egl_init_config_and_surface(egl, pbuffer_width, pbuffer_height);
    egl_check(egl, "init config and surface");

    egl_init_context(egl);
    egl_check(egl, "init context");

    egl_init_gl(egl);
    egl_check(egl, "init gl");
}

static inline void
egl_cleanup(struct egl *egl)
{
    egl_check(egl, "cleanup");

    egl->MakeCurrent(egl->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    egl->DestroyContext(egl->dpy, egl->ctx);
    egl->DestroySurface(egl->dpy, egl->surf);

    egl_cleanup_bo_allocator(egl);

    egl->Terminate(egl->dpy);
    egl->ReleaseThread();

    dlclose(egl->handle);
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

static inline struct egl_bo *
egl_create_bo(struct egl *egl, const struct egl_bo_info *info)
{
    struct egl_bo *bo = calloc(1, sizeof(*bo));
    if (!bo)
        egl_die("failed to alloc bo");
    bo->info = *info;

    egl_alloc_bo_storage(egl, bo);

    return bo;
}

static inline struct egl_bo *
egl_create_bo_from_ppm(struct egl *egl, const void *ppm_data, size_t ppm_size)
{
    int width;
    int height;
    ppm_data = egl_parse_ppm(ppm_data, ppm_size, &width, &height);

    const struct egl_bo_info bo_info = {
        .width = width,
        .height = height,
        .drm_format = DRM_FORMAT_ABGR8888,
        .drm_modifier = DRM_FORMAT_MOD_INVALID,
    };
    struct egl_bo *bo = egl_create_bo(egl, &bo_info);

    void *map = egl_map_bo_storage(egl, bo);
    for (int y = 0; y < height; y++) {
        uint8_t *dst = map + bo->stride * y;
        for (int x = 0; x < width; x++) {
            memcpy(dst, ppm_data, 3);
            dst[3] = 0xff;

            ppm_data += 3;
            dst += 4;
        }
    }

    egl_unmap_bo_storage(egl, bo);

    return bo;
}

static inline void
egl_destroy_bo(struct egl *egl, struct egl_bo *bo)
{
    egl_free_bo_storage(egl, bo);
    free(bo);
}

static inline struct egl_image *
egl_create_image(struct egl *egl, const struct egl_bo *bo)
{
    struct egl_image *img = calloc(1, sizeof(*img));
    if (!img)
        egl_die("failed to alloc img");

    img->img = egl_wrap_bo_storage(egl, bo);
    if (img->img == EGL_NO_IMAGE)
        egl_die("failed to create img");

    return img;
}

static inline void
egl_destroy_image(struct egl *egl, struct egl_image *img)
{
    egl->DestroyImage(egl->dpy, img->img);
    free(img);
}

#endif /* EGLUTIL_H */
