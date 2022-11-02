/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

/* This test clears a linear color image and dumps it to a file. */

#include "eglutil.h"

struct clear_test {
    uint32_t width;
    uint32_t height;
    float color[4];

    struct egl egl;
};

static void
clear_test_init(struct clear_test *test)
{
    egl_init(&test->egl, test->width, test->height);
}

static void
clear_test_cleanup(struct clear_test *test)
{
    egl_cleanup(&test->egl);
}

static void
clear_test_draw(struct clear_test *test)
{
    struct egl_gl *gl = &test->egl.gl;

    gl->ClearColor(test->color[0], test->color[1], test->color[2], test->color[3]);
    gl->Clear(GL_COLOR_BUFFER_BIT);
    egl_check(&test->egl, "clear");

    egl_dump_image(&test->egl, test->width, test->height, "rt.ppm");
}

int
main(void)
{
    struct clear_test test = {
        .width = 320,
        .height = 240,
        .color = { 1.0f, 0.0f, 0.0f, 1.0f },
    };

    clear_test_init(&test);
    clear_test_draw(&test);
    clear_test_cleanup(&test);

    return 0;
}
