/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"

static const char tex_test_vs[] = {
#include "tex_test.vert.inc"
};

static const char tex_test_fs[] = {
#include "tex_test.frag.inc"
};

static const unsigned char tex_test_ppm[] = {
#include "tex_test.ppm.inc"
};

static const float tex_test_vertices[4][8] = {
    {
        -1.0f, /* x */
        -1.0f, /* y */
        0.0f,  /* u */
        0.0f,  /* v */
        1.0f,  /* r */
        1.0f,  /* g */
        1.0f,  /* b */
        1.0f,  /* a */
    },
    {
        1.0f,
        -1.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
    },
    {
        -1.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
    },
    {
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
    },
};

struct tex_test {
    uint32_t width;
    uint32_t height;

    struct egl egl;

    GLuint tex;

    struct egl_program *prog;

    struct egl_image *img;
};

static void
tex_test_init(struct tex_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    const struct egl_init_params params = {
        .pbuffer_width = test->width,
        .pbuffer_height = test->height,
    };
    egl_init(egl, &params);

    gl->GenTextures(1, &test->tex);
    gl->BindTexture(GL_TEXTURE_2D, test->tex);
    gl->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    egl_teximage_2d_from_ppm(egl, GL_TEXTURE_2D, tex_test_ppm, sizeof(tex_test_ppm));

    test->prog = egl_create_program(egl, tex_test_vs, tex_test_fs);

    egl_check(egl, "init");
}

static void
tex_test_cleanup(struct tex_test *test)
{
    struct egl *egl = &test->egl;

    egl_check(egl, "cleanup");

    egl_destroy_program(egl, test->prog);
    egl_cleanup(egl);
}

static void
tex_test_draw(struct tex_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    gl->Clear(GL_COLOR_BUFFER_BIT);
    egl_check(egl, "clear");

    gl->UseProgram(test->prog->prog);
    gl->ActiveTexture(GL_TEXTURE0);
    gl->BindTexture(GL_TEXTURE_2D, test->tex);

    gl->VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(tex_test_vertices[0]),
                            tex_test_vertices);
    gl->EnableVertexAttribArray(0);

    gl->VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(tex_test_vertices[0]),
                            &tex_test_vertices[0][2]);
    gl->EnableVertexAttribArray(1);

    gl->VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(tex_test_vertices[0]),
                            &tex_test_vertices[0][4]);
    gl->EnableVertexAttribArray(2);

    egl_check(egl, "setup");

    gl->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    egl_check(egl, "draw");

    egl_dump_image(&test->egl, test->width, test->height, "rt.ppm");
}

int
main(int argc, const char **argv)
{
    struct tex_test test = {
        .width = 480,
        .height = 360,
    };

    tex_test_init(&test);
    tex_test_draw(&test);
    tex_test_cleanup(&test);

    return 0;
}
