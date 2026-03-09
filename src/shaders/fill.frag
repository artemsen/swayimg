// SPDX-License-Identifier: MIT
// Fragment shader: solid color fill.

#version 450

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
    layout(offset = 32) vec4 color;
} pc;

void main()
{
    out_color = pc.color;
}
