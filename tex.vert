#version 320 es

/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in mediump vec4 in_color;

layout(location = 0) out mediump vec4 out_color;
layout(location = 1) out vec2 out_texcoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(in_position, 0.0, 1.0);
    out_color = in_color;
    out_texcoord = in_texcoord;
}
