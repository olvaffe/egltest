#version 320 es

/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

precision mediump float;

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = in_color;
}
