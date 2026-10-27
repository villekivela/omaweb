#version 440

// The halation's copy as the website takes it (website/scene.js): the Scene's
// picture averaged down a mipmap level at a time, with the refresh band and
// the flicker the website draws into its picture before it takes the copy,
// and cut at the threshold, each channel brighter than `threshold` counted
// and rescaled to 0 to 1. The glass blurs it after.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // The mipmap level a texel of this pass averages.
    float lod;
    float threshold;
    // As the glass's: the band's middle and half height in the Scene's
    // pixels, its strength and colour, and the flicker.
    float sceneHeight;
    float bandY;
    float bandReach;
    float bandStrength;
    vec4 bandColour;
    float flicker;
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    vec3 colour = textureLod(source, qt_TexCoord0, lod).rgb;
    float band = max(0.0, 1.0 - abs(qt_TexCoord0.y * sceneHeight - bandY) / bandReach);
    colour = mix(colour, bandColour.rgb, band * bandStrength) * (1.0 - flicker);
    fragColor = vec4(max(colour - threshold, 0.0) / (1.0 - threshold), 1.0) * qt_Opacity;
}
