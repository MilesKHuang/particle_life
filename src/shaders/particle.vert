#version 430 core
layout(location = 0) in vec2 aPos;

// Read particle type from the same SSBO used by compute shader
layout(std430, binding = 2) readonly buffer Types { uint data[]; } types;

out vec4 vColor;

uniform vec2 uOffset = vec2(0.0, 0.0);
uniform vec2 uScale = vec2(1.0, 1.0);
uniform vec4 uColors[10];
uniform float uPointSizes[10];

void main() {
    vec2 pos = (aPos + uOffset) * uScale;
    pos = pos / vec2(1280.0, 720.0);
    gl_Position = vec4(pos, 0.0, 1.0);
    // Static point size per type, derived from force matrix involvement
    int type = int(types.data[gl_VertexID]);
    gl_PointSize = uPointSizes[type];
    vColor = uColors[type];
}
