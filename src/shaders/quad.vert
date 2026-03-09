// SPDX-License-Identifier: MIT
// Vertex shader for positioned/scaled quad rendering.
// Generates a quad from vertex index (no vertex buffer needed).
// Push constants define position and size in normalized device coordinates.

#version 450

layout(push_constant) uniform PushConstants {
    vec2 pos;  // top-left position in pixels
    vec2 size; // size in pixels
    vec2 viewport; // viewport size in pixels
} pc;

layout(location = 0) out vec2 frag_uv;

void main() {
    // Generate quad vertices from vertex index (triangle strip, 4 vertices)
    // Vertex 0: (0,0) TL, 1: (1,0) TR, 2: (0,1) BL, 3: (1,1) BR
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 0.5;
    frag_uv = uv;

    // Convert from pixel coordinates to NDC [-1, 1]
    vec2 pixel_pos = pc.pos + uv * pc.size;
    vec2 ndc = (pixel_pos / pc.viewport) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
}
