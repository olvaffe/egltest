/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"

#include <time.h>

static const char timestamp_test_vs[] = {
#include "timestamp_test.vert.inc"
};

static const char timestamp_test_fs[] = {
#include "timestamp_test.frag.inc"
};

static const float timestamp_test_vertices[3][6] = {
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

struct timestamp_test {
    uint32_t width;
    uint32_t height;

    struct egl egl;

    struct egl_program *prog;
    GLuint query_begin;
    GLuint query_end;
};

static void
timestamp_test_init(struct timestamp_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    const struct egl_init_params params = {
        .pbuffer_width = test->width,
        .pbuffer_height = test->height,
    };
    egl_init(egl, &params);

    if (!strstr(egl->gl_exts, "GL_EXT_disjoint_timer_query"))
        egl_die("no GL_EXT_disjoint_timer_query support");

    test->prog = egl_create_program(egl, timestamp_test_vs, timestamp_test_fs);

    gl->GenQueries(1, &test->query_begin);
    gl->GenQueries(1, &test->query_end);

    egl_check(egl, "init");
}

static void
timestamp_test_cleanup(struct timestamp_test *test)
{
    struct egl *egl = &test->egl;

    egl_check(egl, "cleanup");

    egl_destroy_program(egl, test->prog);
    egl_cleanup(egl);
}

static uint64_t
get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000llu + ts.tv_nsec;
}

static void
timestamp_test_draw(struct timestamp_test *test)
{
    struct egl *egl = &test->egl;
    struct egl_gl *gl = &egl->gl;

    gl->Clear(GL_COLOR_BUFFER_BIT);
    egl_check(egl, "clear");

    gl->UseProgram(test->prog->prog);

    gl->VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(timestamp_test_vertices[0]),
                            timestamp_test_vertices);
    gl->EnableVertexAttribArray(0);

    gl->VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(timestamp_test_vertices[0]),
                            &timestamp_test_vertices[0][2]);
    gl->EnableVertexAttribArray(1);

    egl_check(egl, "setup");

    gl->QueryCounterEXT(test->query_begin, GL_TIMESTAMP_EXT);
    gl->DrawArraysInstanced(GL_TRIANGLES, 0, 3, 10000);
    gl->QueryCounterEXT(test->query_end, GL_TIMESTAMP_EXT);
    egl_check(egl, "draw");

    GLint64 get_begin;
    GLint64 get_end;
    const uint64_t cpu_begin = get_time_ns();
    gl->GetInteger64v(GL_TIMESTAMP_EXT, &get_begin);
    gl->Finish();
    const uint64_t cpu_end = get_time_ns();
    gl->GetInteger64v(GL_TIMESTAMP_EXT, &get_end);

    GLint64 gpu_begin;
    GLint64 gpu_end;
    gl->GetQueryObjecti64vEXT(test->query_begin, GL_QUERY_RESULT, &gpu_begin);
    gl->GetQueryObjecti64vEXT(test->query_end, GL_QUERY_RESULT, &gpu_end);

    egl_log("cpu time %dms, gpu time %dms, get time %dms", (int)(cpu_end - cpu_begin) / 1000000,
            (int)(gpu_end - gpu_begin) / 1000000, (int)(get_end - get_begin) / 1000000);

    egl_log("get begin %d.%09ds < gpu begin %d.%09ds < gpu end %d.%09d < get end %d.%09d",
            (int)(get_begin / 1000000000), (int)(get_begin % 1000000000),
            (int)(gpu_begin / 1000000000), (int)(gpu_begin % 1000000000),
            (int)(gpu_end / 1000000000), (int)(gpu_end % 1000000000), (int)(get_end / 1000000000),
            (int)(get_end % 1000000000));

    if (true) {
        const int loop_count = 10;
        const int loop_delay = 200;
        egl_log("Calling glGetInteger64v(GL_TIMESTAMP_EXT) %d times with %dms delay", loop_count,
                loop_delay);
        for (int i = 0; i < loop_count; i++) {
            GLint64 gpu_now;

            gl->GetInteger64v(GL_TIMESTAMP_EXT, &gpu_now);
            egl_log("gpu time is %d.%09ds", (int)(gpu_now / 1000000000),
                    (int)(gpu_now % 1000000000));
            usleep(loop_delay * 1000);
        }
    }
}

int
main(int argc, const char **argv)
{
    struct timestamp_test test = {
        .width = 480,
        .height = 360,
    };

    timestamp_test_init(&test);
    timestamp_test_draw(&test);
    timestamp_test_cleanup(&test);

    return 0;
}
