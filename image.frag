#version 320 es
#extension GL_OES_EGL_image_external_essl3 : require

/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

precision mediump float;

layout(location = 1, binding = 0) uniform samplerExternalOES tex;
layout(location = 0) in vec2 in_texcoord;
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = texture(tex, in_texcoord);
}
