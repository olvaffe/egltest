/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"

int
main(void)
{
    struct egl egl;

    egl_init(&egl, 0, 0);
    egl_dump_formats(&egl);
    egl_cleanup(&egl);

    return 0;
}
