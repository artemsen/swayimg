// SPDX-License-Identifier: MIT
// Fragment shader: checkerboard grid pattern for transparent image backgrounds.

#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
    layout(offset = 8) vec2 size;
    layout(offset = 32) vec4 color1;
    layout(offset = 48) vec4 color2;
    layout(offset = 64) float cell_size;
} pc;

void main()
{
    vec2 pixel = frag_uv * pc.size;
    float cx = floor(pixel.x / pc.cell_size);
    float cy = floor(pixel.y / pc.cell_size);
    bool even = mod(cx + cy, 2.0) < 1.0;
    out_color = even ? pc.color1 : pc.color2;
}
