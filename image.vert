#version 320 es

/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

layout(location = 0) uniform mat4 tex_transform;
layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_texcoord;
layout(location = 0) out vec2 out_texcoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    gl_Position = in_position;
    out_texcoord = (tex_transform * in_texcoord).xy;
}
