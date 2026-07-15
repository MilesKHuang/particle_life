#version 430 core
in vec4 vColor;
out vec4 FragColor;

void main() {
    // Soft circle point
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    if (dist > 0.5) discard;
    float alpha = 1.0 - smoothstep(0.3, 0.5, dist);
    FragColor = vec4(vColor.rgb, alpha * 0.85);
}
