#version 440

// A vector screen's phosphor persistence: each frame keeps the brighter of
// the Scene's picture and the last frame, faded by `fade`.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float fade;
};

layout(binding = 1) uniform sampler2D current;
layout(binding = 2) uniform sampler2D previous;

void main()
{
    vec3 now = texture(current, qt_TexCoord0).rgb;
    vec3 before = texture(previous, qt_TexCoord0).rgb * fade;
    fragColor = vec4(max(now, before), 1.0) * qt_Opacity;
}
