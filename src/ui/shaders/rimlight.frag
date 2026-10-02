#version 440

// The light a Scene casts on a plate's rim, as the website's stylesheet draws
// it on the Omnibar: a radial gradient on an ellipse centred on the sun, seen
// through the plate's one-pixel border, and a bloom, a wider band across the
// edge blurred and added to what is behind it as light is. RimLight.qml says
// where each amount comes from.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // This item's size, and the plate inside it, as x, y, width and height,
    // all in logical pixels.
    vec2 itemSize;
    vec4 plateArea;
    float plateRadius;
    // The sun's centre and the gradient's ellipse radii.
    vec2 sunCentre;
    vec2 radii;
    // Up to six stops, their colours premultiplied, the positions of the
    // first four in `at0` and of the last two in `at1`.
    vec4 stop0;
    vec4 stop1;
    vec4 stop2;
    vec4 stop3;
    vec4 stop4;
    vec4 stop5;
    vec4 at0;
    vec2 at1;
    float stopCount;
    // The bloom's band across the edge, its blur's sigma, and its opacity.
    float bloomWidth;
    float bloomBlur;
    float bloomOpacity;
    // 0 for the rim and the bloom outside the border's inner edge, 1 for the
    // bloom inside it, which is drawn under the plate's text.
    float innerHalf;
};

// Distance from a rounded rectangle's edge, negative inside.
float edgeDistance(vec2 p, vec2 centre, vec2 halfSize, float radius)
{
    vec2 q = abs(p - centre) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

// The share of a Gaussian blur of sigma 1 that lies below `x`.
float below(float x)
{
    return 0.5 + 0.5 * tanh(0.7978846 * (x + 0.044715 * x * x * x));
}

vec4 stopColour(int index)
{
    return index == 0 ? stop0 : index == 1 ? stop1 : index == 2 ? stop2 : index == 3 ? stop3
        : index == 4 ? stop4 : stop5;
}

float stopPosition(int index)
{
    return index < 4 ? at0[index] : at1[index - 4];
}

// The gradient at `t`, a share of the way out along the ellipse: before the
// first stop its colour, after the last its colour, and between two a blend of
// them, premultiplied, as CSS interpolates.
vec4 sunlight(float t)
{
    int count = int(stopCount);
    if (t <= stopPosition(0))
        return stop0;
    for (int index = 1; index < 6; ++index) {
        if (index >= count)
            break;
        float from = stopPosition(index - 1);
        float to = stopPosition(index);
        if (t <= to)
            return mix(stopColour(index - 1), stopColour(index),
                (t - from) / max(to - from, 1e-5));
    }
    return stopColour(count - 1);
}

void main()
{
    vec2 p = qt_TexCoord0 * itemSize;
    vec4 light = sunlight(length((p - sunCentre) / radii));
    float d = edgeDistance(p, plateArea.xy + plateArea.zw * 0.5, plateArea.zw * 0.5, plateRadius);

    // The border: the pixel inside the plate's edge.
    float rim = clamp(min(d + 1.0, -d) + 0.5, 0.0, 1.0);

    // The bloom's band is centred on the border's inner edge, as the website's
    // ring is, then blurred.
    float across = d + 1.0;
    float halfBand = bloomWidth * 0.5;
    float bloom = bloomBlur > 0.0
        ? below((across + halfBand) / bloomBlur) - below((across - halfBand) / bloomBlur)
        : step(abs(across), halfBand);

    float inside = 1.0 - step(0.0, across);
    rim *= 1.0 - innerHalf;
    bloom *= mix(1.0 - inside, inside, innerHalf);

    // The rim is laid over what is behind it; the bloom carries no alpha, so
    // it adds to it.
    fragColor = vec4(light.rgb * (rim + bloom * bloomOpacity), light.a * rim) * qt_Opacity;
}
