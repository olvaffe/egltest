/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This is to trigger st_save_zombie_shader/st_context_free_zombie_objects of
 * mesa drivers.
 */

#include "eglutil.h"

#include <strings.h>
#include <threads.h>

static const char multithread_test_vs[] = {
#include "multithread_test.vert.inc"
};

static const char multithread_test_fs[] = {
#include "multithread_test.frag.inc"
};

static const float multithread_test_vertices[4][2] = {
    {
        -1.0f, /* x */
        -1.0f, /* y */
    },
    {
        1.0f,
        -1.0f,
    },
    {
        -1.0f,
        1.0f,
    },
    {
        1.0f,
        1.0f,
    },
};

#define IMAGE_COUNT 2

struct multithread_test {
    uint32_t width;
    uint32_t height;

    struct egl egl;

    mtx_t mtx;

    struct egl_image *imgs[IMAGE_COUNT];
    GLuint texs[IMAGE_COUNT];

    struct {
        uint32_t img_mask;
        cnd_t cnd;
    } producer;

    struct {
        thrd_t thrd;
        EGLContext ctx;
        struct egl_program *prog;

        bool stop;
        uint32_t img_mask;
        cnd_t cnd;
    } consumer;
};

static void
multithread_test_consumer_draw(struct multithread_test *test, GLuint tex)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    GLuint fbo;
    gl->GenFramebuffers(1, &fbo);
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl->FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
    if (gl->CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        egl_die("incomplete fbo");

    /* draw with a feedback loop */
    {
        gl->Viewport(0, 0, test->width, test->height);
        gl->UseProgram(test->consumer.prog->prog);
        gl->ActiveTexture(GL_TEXTURE0);
        gl->BindTexture(GL_TEXTURE_2D, tex);

        gl->VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(multithread_test_vertices[0]),
                                multithread_test_vertices);
        gl->EnableVertexAttribArray(0);

        egl_check(egl, "setup");

        gl->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        egl_check(egl, "draw");
    }

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
    gl->DeleteFramebuffers(1, &fbo);

    gl->Flush();
}

static void
multithread_test_consumer_cleanup(struct multithread_test *test)
{
    struct egl *egl = &test->egl;

    egl_destroy_program(egl, test->consumer.prog);

    egl->MakeCurrent(egl->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    egl->DestroyContext(egl->dpy, test->consumer.ctx);
    egl->ReleaseThread();
}

static void
multithread_test_consumer_init(struct multithread_test *test)
{
    struct egl *egl = &test->egl;

    const EGLint ctx_attrs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE,
    };
    EGLContext ctx = egl->CreateContext(egl->dpy, egl->config, egl->ctx, ctx_attrs);
    if (ctx == EGL_NO_CONTEXT)
        egl_die("failed to create a context");
    if (!egl->MakeCurrent(egl->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx))
        egl_die("failed to make context current");

    test->consumer.ctx = ctx;

    test->consumer.prog = egl_create_program(egl, multithread_test_vs, multithread_test_fs);
}

static int
multithread_test_consumer(void *data)
{
    struct multithread_test *test = data;

    multithread_test_consumer_init(test);

    mtx_lock(&test->mtx);
    while (true) {
        while (!test->consumer.img_mask && !test->consumer.stop) {
            if (cnd_wait(&test->consumer.cnd, &test->mtx) != thrd_success)
                egl_die("cnd_wait failed");
        }
        if (test->consumer.stop)
            break;

        const int idx = ffs(test->consumer.img_mask) - 1;
        assert(idx >= 0 && idx < IMAGE_COUNT);
        mtx_unlock(&test->mtx);

        multithread_test_consumer_draw(test, test->texs[idx]);

        mtx_lock(&test->mtx);
        test->consumer.img_mask &= ~(1 << idx);
        test->producer.img_mask |= (1 << idx);
        cnd_signal(&test->producer.cnd);
    }
    mtx_unlock(&test->mtx);

    multithread_test_consumer_cleanup(test);

    return 0;
}

static void
multithread_test_init(struct multithread_test *test)
{
    struct egl *egl = &test->egl;

    egl_init(egl, NULL);
    egl_check(egl, "init");

    if (mtx_init(&test->mtx, mtx_plain) != thrd_success ||
        cnd_init(&test->producer.cnd) != thrd_success ||
        cnd_init(&test->consumer.cnd) != thrd_success)
        egl_die("failed to init mtx/cnd");
}

static void
multithread_test_cleanup(struct multithread_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    gl->DeleteTextures(IMAGE_COUNT, test->texs);
    for (int i = 0; i < IMAGE_COUNT; i++) {
        if (test->imgs[i])
            egl_destroy_image(egl, test->imgs[i]);
    }

    mtx_destroy(&test->mtx);
    cnd_destroy(&test->producer.cnd);
    cnd_destroy(&test->consumer.cnd);

    egl_check(egl, "cleanup");

    egl_cleanup(egl);
}

static void
multithread_test_draw_produce(struct multithread_test *test, int idx)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;
    struct egl_image *img = test->imgs[idx];
    GLuint tex = test->texs[idx];

    if (!img) {
        const struct egl_image_info info = {
            .width = test->width,
            .height = test->height,
            .drm_format = DRM_FORMAT_ABGR8888,
            .rendering = true,
            .sampling = true,
            .force_linear = true,
        };
        img = egl_create_image(egl, &info);

        test->imgs[idx] = img;
    }

    /* destroy EGLImage and GL tex */
    if (tex)
        gl->DeleteTextures(1, &tex);
    egl->DestroyImage(egl->dpy, img->img);

    /* recreate EGLImage and GL tex */
    egl_wrap_image_storage(egl, img);
    gl->GenTextures(1, &tex);
    gl->BindTexture(GL_TEXTURE_2D, tex);
    gl->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->EGLImageTargetTexture2DOES(GL_TEXTURE_2D, img->img);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    test->texs[idx] = tex;

    /* clear the image */
    {
        GLuint fbo;
        gl->GenFramebuffers(1, &fbo);
        gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);
        gl->FramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
        if (gl->CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            egl_die("incomplete fbo");

#if 0
        gl->Clear(GL_COLOR_BUFFER_BIT);
        egl_check(egl, "clear");
#endif

        gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
        gl->DeleteFramebuffers(1, &fbo);
    }
}

static void
multithread_test_draw(struct multithread_test *test)
{
    test->producer.img_mask = (1 << IMAGE_COUNT) - 1;
    if (thrd_create(&test->consumer.thrd, multithread_test_consumer, test) != thrd_success)
        egl_die("thrd_create failed");

    mtx_lock(&test->mtx);
    while (true) {
        while (!test->producer.img_mask) {
            if (cnd_wait(&test->producer.cnd, &test->mtx) != thrd_success)
                egl_die("cnd_wait failed");
        }

        const int idx = ffs(test->producer.img_mask) - 1;
        assert(idx >= 0 && idx < IMAGE_COUNT);
        mtx_unlock(&test->mtx);

        multithread_test_draw_produce(test, idx);

        mtx_lock(&test->mtx);
        test->producer.img_mask &= ~(1 << idx);
        test->consumer.img_mask |= (1 << idx);
        cnd_signal(&test->consumer.cnd);
    }
    mtx_unlock(&test->mtx);

    if (thrd_join(test->consumer.thrd, NULL) != thrd_success)
        egl_die("thrd_join failed");
}

int
main(int argc, const char **argv)
{
    struct multithread_test test = {
        .width = 1280,
        .height = 720,
    };

    multithread_test_init(&test);
    multithread_test_draw(&test);
    multithread_test_cleanup(&test);

    return 0;
}
