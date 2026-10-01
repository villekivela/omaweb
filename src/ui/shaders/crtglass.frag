#version 440

// The CRT glass a Scene can declare, as one pass over the Scene's small
// picture: its pixels shown square, a rolling refresh band, a flicker, the
// bloom of a third-size copy, scanlines and a vignette. The amounts are the
// website's (website/scene.js and the `.crt` layers in website/styles.css), so
// the road reads the same through either glass.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // The Scene's picture in its own pixels.
    vec2 sceneSize;
    // The glass's height in logical pixels, which the scanlines are spaced in.
    float glassHeight;
    // The part of the glass the reader sees, as x, y, width and height from 0
    // to 1, which the vignette is centred in.
    vec4 frame;
    // The refresh band's middle and half height, in the Scene's pixels.
    float bandY;
    float bandReach;
    // How much the band lightens at its middle and the flicker darkens.
    float bandStrength;
    float flicker;
    vec4 bandColour;
};

layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D bloom;

void main()
{
    vec2 uv = qt_TexCoord0;
    vec3 colour = texture(source, uv).rgb;

    // The band is drawn into the picture, so it lands on whole rows of it.
    float row = floor(uv.y * sceneSize.y) + 0.5;
    float band = max(0.0, 1.0 - abs(row - bandY) / bandReach) * bandStrength;
    colour = mix(colour, bandColour.rgb, band);

    colour = mix(colour, texture(bloom, uv).rgb, 0.22);
    colour *= 1.0 - flicker;

    // Every third line of the glass is drawn at 62% of the others.
    if (mod(uv.y * glassHeight, 3.0) >= 2.0)
        colour *= 0.62;

    // Clear to 55% of the way from the frame's middle to its corners, then
    // darkening to 55% darker at them.
    vec2 inFrame = (uv - frame.xy) / frame.zw;
    float reach = length((inFrame - 0.5) * 2.0) / sqrt(2.0);
    colour *= 1.0 - clamp((reach - 0.55) / 0.45, 0.0, 1.0) * 0.55;

    fragColor = vec4(colour, 1.0) * qt_Opacity;
}
