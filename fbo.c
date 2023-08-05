/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"

static const char fbo_test_vs[] = {
#include "fbo_test.vert.inc"
};

static const char fbo_test_fs[] = {
#include "fbo_test.frag.inc"
};

static const float fbo_test_vertices[3][6] = {
    {
        -1.0f, /* x */
        -1.0f, /* y */
        1.0f,  /* r */
        0.0f,  /* g */
        0.0f,  /* b */
        1.0f,  /* a */
    },
    {
        1.0f,
        -1.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
    },
    {
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
    },
};

struct fbo_test {
    uint32_t width;
    uint32_t height;

    struct egl egl;

    struct egl_program *prog;
    struct egl_framebuffer *fb;
};

static void
fbo_test_init(struct fbo_test *test)
{
    struct egl *egl = &test->egl;

    egl_init(egl, NULL);

    test->prog = egl_create_program(egl, fbo_test_vs, fbo_test_fs);
    test->fb = egl_create_framebuffer(egl, test->width, test->height);

    egl_check(egl, "init");
}

static void
fbo_test_cleanup(struct fbo_test *test)
{
    struct egl *egl = &test->egl;

    egl_check(egl, "cleanup");

    egl_destroy_framebuffer(egl, test->fb);
    egl_destroy_program(egl, test->prog);
    egl_cleanup(egl);
}

static void
fbo_test_draw(struct fbo_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    gl->BindFramebuffer(GL_FRAMEBUFFER, test->fb->fbo);

    gl->Viewport(0, 0, test->width, test->height);

    gl->Clear(GL_COLOR_BUFFER_BIT);
    egl_check(egl, "clear");

    gl->UseProgram(test->prog->prog);

    gl->VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(fbo_test_vertices[0]),
                            fbo_test_vertices);
    gl->EnableVertexAttribArray(0);

    gl->VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(fbo_test_vertices[0]),
                            &fbo_test_vertices[0][2]);
    gl->EnableVertexAttribArray(1);

    egl_check(egl, "setup");

    gl->DrawArrays(GL_TRIANGLES, 0, 3);
    egl_check(egl, "draw");

    egl_dump_image(&test->egl, test->width, test->height, "rt.ppm");

    gl->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

int
main(int argc, const char **argv)
{
    struct fbo_test test = {
        .width = 480,
        .height = 360,
    };

    fbo_test_init(&test);
    fbo_test_draw(&test);
    fbo_test_cleanup(&test);

    return 0;
}
