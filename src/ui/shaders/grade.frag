#version 440

// The grade toward the theme's second hue, as a sunset, over a Scene's plain
// picture: the pass the Scene is drawn through when the reader has turned the
// CRT glass off. The glass grades its picture the same way (crtglass.frag),
// so the grade does not depend on it.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // The part of the picture the reader sees, as x, y, width and height from
    // 0 to 1, which the sunset is measured down.
    vec4 frame;
    // The second hue. The grade starts `gradeTop` of the way down the frame
    // and is full `gradeDepth` further down. The highlights take the hue from
    // the brightness `from` to `to` by up to `strength`, as x, y and z, and
    // the midtones the same, where the sunset is deeper.
    vec4 gradeHue;
    float gradeTop;
    float gradeDepth;
    vec3 gradeHighlights;
    vec3 gradeMidtones;
};

layout(binding = 1) uniform sampler2D source;

float luma(vec3 colour)
{
    return dot(colour, vec3(0.2126, 0.7152, 0.0722));
}

void main()
{
    vec4 picture = texture(source, qt_TexCoord0);
    vec3 colour = picture.rgb;

    // The bright parts take the hue at their own brightness, and more of them
    // lower in the frame; the dark ones keep the Scene's colours.
    float level = luma(colour);
    vec3 tinted = gradeHue.rgb * (level / max(luma(gradeHue.rgb), 0.05));
    float down = clamp(((qt_TexCoord0.y - frame.y) / frame.w - gradeTop) / gradeDepth, 0.0, 1.0);
    float amount = max(
        smoothstep(gradeHighlights.x, gradeHighlights.y, level) * gradeHighlights.z * down,
        smoothstep(gradeMidtones.x, gradeMidtones.y, level) * gradeMidtones.z * down * down);
    colour = mix(colour, tinted, amount);

    fragColor = vec4(colour, picture.a) * qt_Opacity;
}
