#version 440

// A translucent ground with a dither in it, so a gradient on the desktop
// behind it does not show as bands (#647). The ground is drawn here rather
// than by the surface, so the noise is added after the ground's alpha and
// reaches the screen at full strength. GroundDither.qml says when it is drawn.
//
// The noise is a hash of the pixel's place in the window: it holds still while
// the window is idle and needs no frame of its own. Each channel takes the
// sum of two uniform values, a triangular spread of one output level either
// way, which averages to nothing.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // This item's size in logical pixels and the corner radius it is kept
    // inside.
    vec2 itemSize;
    float radius;
    // The ground's colour, premultiplied by its alpha.
    vec4 premultiplied;
};

// Distance from a rounded rectangle's edge, negative inside.
float edgeDistance(vec2 p, vec2 centre, vec2 halfSize, float corner)
{
    vec2 q = abs(p - centre) - halfSize + corner;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - corner;
}

// Three values in [0, 1) from a point, Dave Hoskins's "hash without sine":
// arithmetic only, as GLSL ES 1.0 has no integers to mix bits with.
vec3 hash(vec2 point)
{
    vec3 p = fract(vec3(point.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yzz) * p.zyx);
}

void main()
{
    vec2 p = qt_TexCoord0 * itemSize;
    float d = edgeDistance(p, itemSize * 0.5, itemSize * 0.5, radius);
    // A quarter pixel past the edge, where a Rectangle with a border fills
    // under its inner edge; measured against one under llvmpipe.
    float coverage = clamp(0.75 - d / max(fwidth(d), 1e-4), 0.0, 1.0);

    vec2 pixel = floor(gl_FragCoord.xy);
    vec3 first = hash(pixel);
    vec3 second = hash(pixel + vec2(1013.0, 2011.0));
    vec3 noise = (first + second - 1.0) / 255.0;

    fragColor = vec4(premultiplied.rgb + noise, premultiplied.a) * coverage * qt_Opacity;
}
