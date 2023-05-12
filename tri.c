/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"

static const char tri_test_vs[] = {
#include "tri_test.vert.inc"
};

static const char tri_test_fs[] = {
#include "tri_test.frag.inc"
};

static const float tri_test_vertices[3][6] = {
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

struct tri_test {
    uint32_t width;
    uint32_t height;

    struct egl egl;

    struct egl_program *prog;
};

static void
tri_test_init(struct tri_test *test)
{
    struct egl *egl = &test->egl;

    const struct egl_init_params params = {
        .pbuffer_width = test->width,
        .pbuffer_height = test->height,
    };
    egl_init(egl, &params);

    test->prog = egl_create_program(egl, tri_test_vs, tri_test_fs);

    egl_check(egl, "init");
}

static void
tri_test_cleanup(struct tri_test *test)
{
    struct egl *egl = &test->egl;

    egl_check(egl, "cleanup");

    egl_destroy_program(egl, test->prog);
    egl_cleanup(egl);
}

static void
tri_test_draw(struct tri_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    gl->Clear(GL_COLOR_BUFFER_BIT);
    egl_check(egl, "clear");

    gl->UseProgram(test->prog->prog);

    gl->VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(tri_test_vertices[0]),
                            tri_test_vertices);
    gl->EnableVertexAttribArray(0);

    gl->VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(tri_test_vertices[0]),
                            &tri_test_vertices[0][2]);
    gl->EnableVertexAttribArray(1);

    egl_check(egl, "setup");

    gl->DrawArrays(GL_TRIANGLES, 0, 3);
    egl_check(egl, "draw");

    egl_dump_image(&test->egl, test->width, test->height, "rt.ppm");
}

int
main(int argc, const char **argv)
{
    struct tri_test test = {
        .width = 480,
        .height = 360,
    };

    tri_test_init(&test);
    tri_test_draw(&test);
    tri_test_cleanup(&test);

    return 0;
}
