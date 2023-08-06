#version 320 es

/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

precision mediump float;

layout(location = 0, binding = 0) uniform sampler2D tex;
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = texelFetch(tex, ivec2(gl_FragCoord.xy), 0);
}
