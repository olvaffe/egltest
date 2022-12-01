#version 320 es

/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

precision mediump float;

layout(location = 0, binding = 0) uniform sampler2D tex;
layout(location = 0) in vec4 in_color;
layout(location = 1) in highp vec2 in_texcoord;
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = in_color * texture(tex, in_texcoord);
}
