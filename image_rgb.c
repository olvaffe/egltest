/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"

/* for image_rgb_test_ppm_data */
#include "image_rgb_test.ppm.inc"

static const char image_rgb_test_vs[] = "#version 320 es\n"
                                        "layout(location = 0) uniform mat4 tex_transform;\n"
                                        "layout(location = 0) in vec4 in_position;\n"
                                        "layout(location = 1) in vec4 in_texcoord;\n"
                                        "layout(location = 0) out vec2 out_texcoord;\n"
                                        "out gl_PerVertex {\n"
                                        "   vec4 gl_Position;\n"
                                        "};\n"
                                        "\n"
                                        "void main()\n"
                                        "{\n"
                                        "    gl_Position = in_position;\n"
                                        "    out_texcoord = (tex_transform * in_texcoord).xy;\n"
                                        "}\n";

static const char image_rgb_test_fs[] =
    "#version 320 es\n"
    "#extension GL_OES_EGL_image_external : require\n"
    "precision mediump float;\n"
    "layout(location = 1, binding = 0) uniform samplerExternalOES tex;\n"
    "layout(location = 0) in vec2 in_texcoord;\n"
    "layout(location = 0) out vec4 out_color;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    out_color = texture2D(tex, in_texcoord);\n"
    "}\n";

static const float image_rgb_test_vertices[4][5] = {
    {
        -1.0f, /* x */
        -1.0f, /* y */
        0.0f,  /* z */
        0.0f,  /* u */
        0.0f,  /* v */
    },
    {
        1.0f,
        -1.0f,
        0.0f,
        1.0f,
        0.0f,
    },
    {
        -1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
    },
    {
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
    },
};

static const float image_rgb_test_tex_transform[4][4] = {
#if 1
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
#else
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, -0.828125f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.8359375f, 0.0f, 1.0f },
#endif
};

struct image_rgb_test {
    uint32_t width;
    uint32_t height;

    struct egl egl;

    GLenum tex_target;
    GLuint tex;

    struct egl_program *prog;

    struct egl_image *img;
};

static void
image_rgb_test_init(struct image_rgb_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    egl_init(egl, test->width, test->height);

    if (!strstr(egl->gl_exts, "GL_OES_EGL_image_external"))
        egl_die("no GL_OES_EGL_image_external");

    test->tex_target = GL_TEXTURE_EXTERNAL_OES;
    gl->GenTextures(1, &test->tex);
    gl->BindTexture(test->tex_target, test->tex);
    gl->TexParameterf(test->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameterf(test->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(test->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(test->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    test->prog = egl_create_program(egl, image_rgb_test_vs, image_rgb_test_fs);

    test->img = egl_create_image_from_ppm(egl, image_rgb_test_ppm_data,
                                          sizeof(image_rgb_test_ppm_data), false);
    gl->EGLImageTargetTexture2DOES(test->tex_target, test->img->img);

    egl_check(egl, "init");
}

static void
image_rgb_test_cleanup(struct image_rgb_test *test)
{
    struct egl *egl = &test->egl;

    egl_check(egl, "cleanup");

    egl_destroy_program(egl, test->prog);
    egl_destroy_image(egl, test->img);
    egl_cleanup(egl);
}

static void
image_rgb_test_draw(struct image_rgb_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    gl->Clear(GL_COLOR_BUFFER_BIT);
    egl_check(egl, "clear");

    gl->UseProgram(test->prog->prog);
    gl->ActiveTexture(GL_TEXTURE0);
    gl->BindTexture(test->tex_target, test->tex);

    gl->UniformMatrix4fv(0, 1, false, (const float *)image_rgb_test_tex_transform);

    gl->VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(image_rgb_test_vertices[0]),
                            image_rgb_test_vertices);
    gl->EnableVertexAttribArray(0);

    gl->VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(image_rgb_test_vertices[0]),
                            &image_rgb_test_vertices[0][3]);
    gl->EnableVertexAttribArray(1);

    egl_check(egl, "setup");

    gl->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    egl_check(egl, "draw");

    egl_dump_image(&test->egl, test->width, test->height, "rt.ppm");
}

int
main(void)
{
    struct image_rgb_test test = {
        .width = 480,
        .height = 360,
    };

    image_rgb_test_init(&test);
    image_rgb_test_draw(&test);
    image_rgb_test_cleanup(&test);

    return 0;
}
